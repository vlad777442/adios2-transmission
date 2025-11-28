/*
 * ADIOS2 BP File Relay Sender
 * Reads data from an existing BP file and transmits to remote receiver
 */

#include <adios2.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <thread>
#include <mpi.h>
#include <string>

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Parse command line arguments
    if (argc < 2) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0] << " <input_bp_file> [output_contact_name]" << std::endl;
            std::cerr << "Example: " << argv[0] << " /path/to/gs-2gb.bp data-transfer" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string contactFile = "data-transfer";
    if (argc > 2) {
        contactFile = argv[2];
    }
    
    try {
        adios2::ADIOS adios(MPI_COMM_WORLD);
        
        // === READ SIDE: Open input BP file ===
        adios2::IO ioRead = adios.DeclareIO("ReadIO");
        ioRead.SetEngine("BP5");
        adios2::Engine reader = ioRead.Open(inputFile, adios2::Mode::Read);
        
        // === WRITE SIDE: Configure SST for WAN transfer ===
        adios2::IO ioWrite = adios.DeclareIO("TransferIO");
        ioWrite.SetEngine("SST");
        ioWrite.SetParameters({
            {"RendezvousReaderCount", "1"},
            {"QueueLimit", "5"},
            {"QueueFullPolicy", "Block"},
            {"ControlTransport", "sockets"},
            {"DataTransport", "sockets"},
            {"OpenTimeoutSecs", "300"},
            {"MarshalMethod", "BP5"}
        });
        
        adios2::Engine writer = ioWrite.Open(contactFile, adios2::Mode::Write);
        
        if (rank == 0) {
            std::cout << "=== ADIOS2 BP File Relay Sender ===" << std::endl;
            std::cout << "Input BP file: " << inputFile << std::endl;
            std::cout << "Contact file: " << contactFile << ".sst" << std::endl;
            std::cout << "MPI Ranks: " << size << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            
            // Wait a moment for SST file to be created
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Read and display SST connection string
            std::ifstream sstFile(contactFile + ".sst");
            if (sstFile.is_open()) {
                std::string line;
                std::getline(sstFile, line); // Skip first line (#ADIOS2-SST v0)
                std::getline(sstFile, line); // Get connection string
                sstFile.close();
                
                if (!line.empty()) {
                    std::cout << "\n*** SST CONNECTION STRING ***" << std::endl;
                    std::cout << line << std::endl;
                    std::cout << "\nCopy the above string and run on receiver:" << std::endl;
                    std::cout << "  ./build/receiver \"" << line << "\"" << std::endl;
                    std::cout << std::string(60, '=') << std::endl;
                    std::cout << "\nPress Enter to continue after starting receiver..." << std::endl;
                    std::cin.get();
                }
            }
            
            std::cout << "Waiting for receiver to connect..." << std::endl;
            std::cout << std::string(60, '=') << std::endl;
        }
        
        // Synchronize all ranks before proceeding
        MPI_Barrier(MPI_COMM_WORLD);
        
        auto overallStart = std::chrono::high_resolution_clock::now();
        size_t stepCount = 0;
        double totalDataMB = 0.0;
        
        // Define output variables once (before any steps)
        adios2::Variable<double> varU_out;
        adios2::Variable<double> varV_out;
        adios2::Variable<int32_t> varStep_out;
        
        size_t totalZ = 0, dimY = 0, dimX = 0;
        size_t zStart = 0, zCount = 0;
        
        // Get dimensions from first step
        if (reader.BeginStep() == adios2::StepStatus::OK) {
            auto varU_in = ioRead.InquireVariable<double>("U");
            if (varU_in) {
                auto shape = varU_in.Shape();
                if (shape.size() == 3) {
                    totalZ = shape[0];
                    dimY = shape[1];
                    dimX = shape[2];
                    
                    size_t zPerRank = totalZ / size;
                    size_t remainder = totalZ % size;
                    zStart = rank * zPerRank + std::min(static_cast<size_t>(rank), remainder);
                    zCount = zPerRank + (rank < remainder ? 1 : 0);
                    
                    // Define variables once
                    varU_out = ioWrite.DefineVariable<double>(
                        "U",
                        {totalZ, dimY, dimX},
                        {zStart, 0, 0},
                        {zCount, dimY, dimX}
                    );
                    varV_out = ioWrite.DefineVariable<double>(
                        "V",
                        {totalZ, dimY, dimX},
                        {zStart, 0, 0},
                        {zCount, dimY, dimX}
                    );
                }
            }
            if (rank == 0) {
                varStep_out = ioWrite.DefineVariable<int32_t>("step");
            }
            reader.EndStep();
        }
        
        // Process each step from the input file
        while (reader.BeginStep() == adios2::StepStatus::OK) {
            auto stepStart = std::chrono::high_resolution_clock::now();
            
            if (rank == 0) {
                std::cout << "Processing step " << stepCount << "..." << std::endl;
            }
            
            writer.BeginStep();
            
            double stepDataMB = 0.0;
            
            // Read and transmit variable U
            auto varU_in = ioRead.InquireVariable<double>("U");
            if (varU_in && zCount > 0) {
                std::vector<double> dataU(zCount * dimY * dimX);
                varU_in.SetSelection({{zStart, 0, 0}, {zCount, dimY, dimX}});
                reader.Get(varU_in, dataU.data());
                reader.PerformGets();
                
                writer.Put(varU_out, dataU.data());
                stepDataMB += (zCount * dimY * dimX * sizeof(double)) / (1024.0 * 1024.0);
            }
            
            // Read and transmit variable V
            auto varV_in = ioRead.InquireVariable<double>("V");
            if (varV_in && zCount > 0) {
                std::vector<double> dataV(zCount * dimY * dimX);
                varV_in.SetSelection({{zStart, 0, 0}, {zCount, dimY, dimX}});
                reader.Get(varV_in, dataV.data());
                reader.PerformGets();
                
                writer.Put(varV_out, dataV.data());
                stepDataMB += (zCount * dimY * dimX * sizeof(double)) / (1024.0 * 1024.0);
            }
            
            // Read and transmit variable step
            auto varStep_in = ioRead.InquireVariable<int32_t>("step");
            if (varStep_in && rank == 0) {
                int32_t stepValue;
                reader.Get(varStep_in, &stepValue);
                reader.PerformGets();
                
                writer.Put(varStep_out, stepValue);
            }
            
            double globalStepDataMB = 0.0;
            MPI_Reduce(&stepDataMB, &globalStepDataMB, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            
            reader.EndStep();
            writer.EndStep();
            
            auto stepEnd = std::chrono::high_resolution_clock::now();
            auto stepDuration = std::chrono::duration<double>(stepEnd - stepStart).count();
            
            if (rank == 0) {
                totalDataMB += globalStepDataMB;
                double throughputMBps = globalStepDataMB / stepDuration;
                
                std::cout << "Step " << std::setw(3) << stepCount 
                          << " | Time: " << std::fixed << std::setprecision(3) << std::setw(8) << stepDuration << " s"
                          << " | Size: " << std::setw(8) << std::setprecision(2) << globalStepDataMB << " MB"
                          << " | Throughput: " << std::setw(8) << std::setprecision(2) << throughputMBps << " MB/s"
                          << std::endl;
            }
            
            stepCount++;
        }
        
        reader.Close();
        writer.Close();
        
        auto overallEnd = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration<double>(overallEnd - overallStart).count();
        
        if (rank == 0) {
            std::cout << std::string(60, '=') << std::endl;
            std::cout << "=== Transfer Complete ===" << std::endl;
            std::cout << "Steps transmitted: " << stepCount << std::endl;
            std::cout << "Total time: " << std::fixed << std::setprecision(3) << totalDuration << " seconds" << std::endl;
            std::cout << "Total data: " << std::setprecision(2) << totalDataMB << " MB" << std::endl;
            std::cout << "Average throughput: " << std::setprecision(2) << (totalDataMB / totalDuration) << " MB/s" << std::endl;
            std::cout << "Average throughput: " << std::setprecision(2) << (totalDataMB / totalDuration * 8.0) << " Mbps" << std::endl;
        }
        
    } catch (std::exception &e) {
        std::cerr << "Error on rank " << rank << ": " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    MPI_Finalize();
    return 0;
}
