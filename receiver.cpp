/*
 * ADIOS2 Data Receiver (Clemson side)
 * Receives data from remote sender and measures transmission metrics
 */

#include <adios2.h>
#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <mpi.h>
#include <algorithm>

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Parse command line arguments
    std::string contactFile = "data-transfer"; // default name
    std::string outputFile = "received_data.bp"; // default output
    bool useContactString = false;
    std::string contactString = "";
    
    if (argc > 1) {
        std::string arg1 = argv[1];
        // Check if it's an SST connection string (starts with typical SST format)
        if (arg1.find("0x") != std::string::npos || arg1.find(":") != std::string::npos) {
            useContactString = true;
            contactString = arg1;
            contactFile = "receiver-connection"; // dummy name
        } else {
            contactFile = arg1;
        }
    }
    if (argc > 2) {
        outputFile = argv[2];
    }
    
    try {
        // Initialize ADIOS2
        adios2::ADIOS adios(MPI_COMM_WORLD);
        
        // Declare IO for reading
        adios2::IO ioRead = adios.DeclareIO("TransferIO");
        
        // Set engine for reading
        ioRead.SetEngine("SST");
        
        // Set parameters for WAN
        ioRead.SetParameters({
            {"ControlTransport", "sockets"},
            {"DataTransport", "sockets"},
            {"OpenTimeoutSecs", "300"}
        });
        
        // If using connection string directly, write it to a temporary file
        if (useContactString && rank == 0) {
            std::ofstream sstFile(contactFile + ".sst");
            sstFile << "#ADIOS2-SST v0\n" << contactString << std::endl;
            sstFile.close();
        }
        MPI_Barrier(MPI_COMM_WORLD); // Wait for file to be written
        
        // Open engine for reading (contact file name can be specified)
        adios2::Engine reader = ioRead.Open(contactFile, adios2::Mode::Read);
        
        // Declare IO for writing received data to BP file
        adios2::IO ioWrite = adios.DeclareIO("WriteIO");
        ioWrite.SetEngine("BP5");
        adios2::Engine writer = ioWrite.Open(outputFile, adios2::Mode::Write);
        
        if (rank == 0) {
            std::cout << "=== ADIOS2 Data Receiver (Clemson) ===" << std::endl;
            if (useContactString) {
                std::cout << "Using SST connection string from command line" << std::endl;
            } else {
                std::cout << "Contact file: " << contactFile << ".sst" << std::endl;
            }
            std::cout << "Output file: " << outputFile << std::endl;
            std::cout << "MPI Ranks: " << size << std::endl;
            std::cout << "Waiting for data from sender..." << std::endl;
            std::cout << std::string(60, '=') << std::endl;
        }
        
        // Metrics tracking
        std::vector<double> stepTimes;
        std::vector<double> stepSizes;
        std::vector<double> stepThroughputs;
        
        auto overallStart = std::chrono::high_resolution_clock::now();
        size_t stepCount = 0;
        
        // Track defined output variables to avoid redefining
        std::map<std::string, adios2::Variable<double>> definedDoubleVars;
        std::map<std::string, adios2::Variable<int32_t>> definedInt32Vars;
        
        // Receive data for all available steps
        while (true) {
            auto stepStatus = reader.BeginStep();
            
            if (stepStatus != adios2::StepStatus::OK) {
                break;  // No more steps available
            }
            
            auto stepStart = std::chrono::high_resolution_clock::now();
            
            // Begin writing step
            writer.BeginStep();
            
            double stepSizeMB = 0.0;
            
            // Get all available variables
            auto variables = ioRead.AvailableVariables();
            
            if (rank == 0 && stepCount == 0) {
                std::cout << "Found " << variables.size() << " variables to receive" << std::endl;
            }
            
            // Process each variable
            for (const auto& varPair : variables) {
                const std::string& varName = varPair.first;
                const auto& varInfo = varPair.second;
                
                auto typeIt = varInfo.find("Type");
                if (typeIt == varInfo.end()) continue;
                const std::string& varType = typeIt->second;
                
                // Handle double variables
                if (varType == "double") {
                    auto varIn = ioRead.InquireVariable<double>(varName);
                    if (!varIn) continue;
                    auto shape = varIn.Shape();
                    if (shape.empty()) continue; // Skip scalars
                    
                    size_t totalSize = 1;
                    for (auto dim : shape) totalSize *= dim;
                    
                    if (shape.size() >= 1) {
                        size_t dim0 = shape[0];
                        size_t elementsPerSlice = totalSize / dim0;
                        size_t slicesPerRank = dim0 / size;
                        size_t remainder = dim0 % size;
                        size_t sliceStart = rank * slicesPerRank + std::min(static_cast<size_t>(rank), remainder);
                        size_t sliceCount = slicesPerRank + (rank < remainder ? 1 : 0);
                        
                        if (sliceCount > 0) {
                            size_t localSize = sliceCount * elementsPerSlice;
                            adios2::Dims start(shape.size(), 0);
                            adios2::Dims count = shape;
                            start[0] = sliceStart;
                            count[0] = sliceCount;
                            
                            varIn.SetSelection({start, count});
                            std::vector<double> data(localSize);
                            reader.Get(varIn, data.data(), adios2::Mode::Sync);
                            
                            if (stepCount == 0) {
                                definedDoubleVars[varName] = ioWrite.DefineVariable<double>(
                                    varName, shape, start, count
                                );
                            }
                            writer.Put(definedDoubleVars[varName], data.data(), adios2::Mode::Sync);
                            stepSizeMB += (localSize * sizeof(double)) / (1024.0 * 1024.0);
                        }
                    }
                }
                // Handle int32_t variables
                else if (varType == "int32_t") {
                    auto varIn = ioRead.InquireVariable<int32_t>(varName);
                    if (!varIn) continue;
                    
                    auto shape = varIn.Shape();
                    if (shape.empty() || (shape.size() == 1 && shape[0] == 1)) {
                        if (rank == 0) {
                            int32_t value;
                            reader.Get(varIn, &value, adios2::Mode::Sync);
                            if (stepCount == 0) {
                                definedInt32Vars[varName] = ioWrite.DefineVariable<int32_t>(varName);
                            }
                            writer.Put(definedInt32Vars[varName], value, adios2::Mode::Sync);
                        }
                    } else {
                        size_t totalSize = 1;
                        for (auto dim : shape) totalSize *= dim;
                        
                        // Distribute along first dimension
                        size_t dim0 = shape[0];
                        size_t elementsPerSlice = totalSize / dim0;
                        size_t slicesPerRank = dim0 / size;
                        size_t remainder = dim0 % size;
                        size_t sliceStart = rank * slicesPerRank + std::min(static_cast<size_t>(rank), remainder);
                        size_t sliceCount = slicesPerRank + (rank < remainder ? 1 : 0);
                        
                        if (sliceCount > 0) {
                            size_t localSize = sliceCount * elementsPerSlice;
                            
                            adios2::Dims start(shape.size(), 0);
                            adios2::Dims count = shape;
                            start[0] = sliceStart;
                            count[0] = sliceCount;
                            
                            varIn.SetSelection({start, count});
                            std::vector<int32_t> data(localSize);
                            reader.Get(varIn, data.data(), adios2::Mode::Sync);
                            
                            if (stepCount == 0) {
                                definedInt32Vars[varName] = ioWrite.DefineVariable<int32_t>(
                                    varName, shape, start, count
                                );
                            }
                            writer.Put(definedInt32Vars[varName], data.data(), adios2::Mode::Sync);
                            stepSizeMB += (localSize * sizeof(int32_t)) / (1024.0 * 1024.0);
                        }
                    }
                }
            }
            
            // Sum up total step size across ranks
            double globalStepSizeMB = 0.0;
            MPI_Reduce(&stepSizeMB, &globalStepSizeMB, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            
            reader.EndStep();
            writer.EndStep();
            
            auto stepEnd = std::chrono::high_resolution_clock::now();
            auto stepDuration = std::chrono::duration<double>(stepEnd - stepStart).count();
            
            if (rank == 0) {
                double throughputMBps = globalStepSizeMB / stepDuration;
                
                std::cout << "Step " << std::setw(3) << stepCount 
                          << " | Time: " << std::fixed << std::setprecision(3) << std::setw(8) << stepDuration << " s"
                          << " | Size: " << std::setw(8) << std::setprecision(2) << globalStepSizeMB << " MB"
                          << " | Throughput: " << std::setw(8) << std::setprecision(2) << throughputMBps << " MB/s"
                          << std::endl;
                
                stepTimes.push_back(stepDuration);
                stepSizes.push_back(globalStepSizeMB);
                stepThroughputs.push_back(throughputMBps);
            }
            
            stepCount++;
        }
        
        reader.Close();
        writer.Close();
        
        auto overallEnd = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration<double>(overallEnd - overallStart).count();
        
        if (rank == 0) {
            std::cout << std::string(60, '=') << std::endl;
            std::cout << "=== Reception Complete ===" << std::endl;
            std::cout << "Total steps received: " << stepCount << std::endl;
            std::cout << "Total time: " << std::fixed << std::setprecision(3) << totalDuration << " seconds" << std::endl;
            
            if (!stepTimes.empty()) {
                // Calculate statistics
                double totalData = 0.0;
                double minTime = stepTimes[0];
                double maxTime = stepTimes[0];
                double minThroughput = stepThroughputs[0];
                double maxThroughput = stepThroughputs[0];
                
                for (size_t i = 0; i < stepTimes.size(); ++i) {
                    totalData += stepSizes[i];
                    minTime = std::min(minTime, stepTimes[i]);
                    maxTime = std::max(maxTime, stepTimes[i]);
                    minThroughput = std::min(minThroughput, stepThroughputs[i]);
                    maxThroughput = std::max(maxThroughput, stepThroughputs[i]);
                }
                
                double avgThroughputMBps = totalData / totalDuration;
                
                std::cout << "\n=== Performance Metrics ===" << std::endl;
                std::cout << "Total data received: " << std::setprecision(2) << totalData << " MB" << std::endl;
                std::cout << "Average throughput: " << std::setprecision(2) << avgThroughputMBps << " MB/s" << std::endl;
                std::cout << "Average throughput: " << std::setprecision(2) << avgThroughputMBps * 8.0 << " Mbps" << std::endl;
                std::cout << "Min/Max step throughput: " << minThroughput << " / " << maxThroughput << " MB/s" << std::endl;
                std::cout << "Min/Max step time: " << std::setprecision(3) << minTime << " / " << maxTime << " s" << std::endl;
                
                // Save detailed metrics to file
                std::ofstream metricsFile("transfer_metrics.csv");
                metricsFile << "Step,Time(s),Size(MB),Throughput(MB/s),Throughput(Mbps)\n";
                for (size_t i = 0; i < stepTimes.size(); ++i) {
                    metricsFile << i << "," 
                               << std::fixed << std::setprecision(6) << stepTimes[i] << ","
                               << std::setprecision(2) << stepSizes[i] << ","
                               << std::setprecision(2) << stepThroughputs[i] << ","
                               << std::setprecision(2) << stepThroughputs[i] * 8.0 << "\n";
                }
                metricsFile.close();
                std::cout << "\nDetailed metrics saved to: transfer_metrics.csv" << std::endl;
                std::cout << "Received data saved to: " << outputFile << std::endl;
            }
        }
        
    } catch (std::exception &e) {
        std::cerr << "Error on rank " << rank << ": " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    MPI_Finalize();
    return 0;
}
