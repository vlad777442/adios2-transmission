/*
 * ADIOS2 Data Receiver (Clemson side)
 * Receives data from remote sender and measures transmission metrics
 */

#include <adios2.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <mpi.h>

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
        
        // Receive data for all available steps
        while (true) {
            auto stepStatus = reader.BeginStep();
            
            if (stepStatus != adios2::StepStatus::OK) {
                break;  // No more steps available
            }
            
            auto stepStart = std::chrono::high_resolution_clock::now();
            
            // Begin writing step
            writer.BeginStep();
            
            // Inquire about variables
            auto varData = ioRead.InquireVariable<double>("data");
            auto varStep = ioRead.InquireVariable<size_t>("step");
            auto varTimestamp = ioRead.InquireVariable<double>("timestamp");
            
            if (!varData) {
                if (rank == 0) {
                    std::cerr << "Error: Variable 'data' not found!" << std::endl;
                }
                break;
            }
            
            // Get the shape of the data
            auto shape = varData.Shape();
            size_t totalSize = shape[0];
            size_t localSize = totalSize / size;
            size_t offset = rank * localSize;
            
            // Handle remainder for last rank
            if (rank == size - 1) {
                localSize = totalSize - offset;
            }
            
            // Set selection for this rank
            varData.SetSelection({{offset}, {localSize}});
            
            // Allocate buffer
            std::vector<double> data(localSize);
            
            // Read data
            reader.Get(varData, data.data());
            
            // Read metadata (only on rank 0)
            size_t step = 0;
            double sendTimestamp = 0.0;
            if (rank == 0 && varStep && varTimestamp) {
                reader.Get(varStep, &step);
                reader.Get(varTimestamp, &sendTimestamp);
            }
            
            // End step (synchronizes the read)
            reader.EndStep();
            
            // Write received data to output BP file
            auto varDataOut = ioWrite.DefineVariable<double>(
                "data",
                {totalSize},
                {offset},
                {localSize}
            );
            writer.Put(varDataOut, data.data());
            
            // Write metadata (only on rank 0)
            if (rank == 0) {
                if (varStep) {
                    auto varStepOut = ioWrite.DefineVariable<size_t>("step");
                    writer.Put(varStepOut, step);
                }
                if (varTimestamp) {
                    auto varTimestampOut = ioWrite.DefineVariable<double>("timestamp");
                    writer.Put(varTimestampOut, sendTimestamp);
                }
            }
            
            writer.EndStep();
            
            auto stepEnd = std::chrono::high_resolution_clock::now();
            auto stepDuration = std::chrono::duration<double>(stepEnd - stepStart).count();
            
            // Calculate metrics
            double stepSizeMB = (totalSize * sizeof(double)) / (1024.0 * 1024.0);
            double throughputMBps = stepSizeMB / stepDuration;
            
            if (rank == 0) {
                auto now = std::chrono::system_clock::now();
                auto recvTimestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
                double latency = recvTimestamp - sendTimestamp;
                
                std::cout << "Step " << std::setw(3) << stepCount 
                          << " | Time: " << std::fixed << std::setprecision(3) << std::setw(8) << stepDuration << " s"
                          << " | Size: " << std::setw(8) << std::setprecision(2) << stepSizeMB << " MB"
                          << " | Throughput: " << std::setw(8) << std::setprecision(2) << throughputMBps << " MB/s";
                
                if (sendTimestamp > 0) {
                    std::cout << " | Latency: " << std::setw(8) << std::setprecision(3) << latency << " s";
                }
                std::cout << std::endl;
                
                stepTimes.push_back(stepDuration);
                stepSizes.push_back(stepSizeMB);
                stepThroughputs.push_back(throughputMBps);
            }
            
            // Validate data (optional - check a few values)
            if (rank == 0 && stepCount == 0) {
                // Verify first few values of first step
                bool dataValid = true;
                for (size_t i = 0; i < std::min(size_t(10), localSize); ++i) {
                    double expected = step + static_cast<double>(i) / totalSize * size;
                    if (std::abs(data[i] - expected) > 1e-6) {
                        dataValid = false;
                        break;
                    }
                }
                if (dataValid) {
                    std::cout << "  ✓ Data validation passed" << std::endl;
                }
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
