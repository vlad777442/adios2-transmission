/*
 * ADIOS2 Data Sender (Utah side)
 * Sends data to remote receiver and tracks transmission metrics
 */

#include <adios2.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <iomanip>
#include <mpi.h>

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Configuration parameters
    const size_t arraySize = 10000000;  // 10M elements (~40 MB per step)
    const size_t numSteps = 10;          // Number of timesteps to send
    
    try {
        // Initialize ADIOS2
        adios2::ADIOS adios(MPI_COMM_WORLD);
        
        // Declare IO with WAN configuration
        adios2::IO io = adios.DeclareIO("TransferIO");
        
        // Set engine parameters for WAN transfer
        // Use SST (Sustainable Staging Transport) for wide-area network transfers
        io.SetEngine("SST");
        
        // Optional: Set parameters for better WAN performance
        io.SetParameters({
            {"RendezvousReaderCount", "1"},
            {"QueueLimit", "5"},
            {"QueueFullPolicy", "Block"}
        });
        
        // Define the data variable
        adios2::Variable<double> varData = io.DefineVariable<double>(
            "data",
            {static_cast<size_t>(size * arraySize)},  // global dimensions
            {static_cast<size_t>(rank * arraySize)},  // offset
            {arraySize}                                 // local dimensions
        );
        
        // Define metadata variables
        adios2::Variable<size_t> varStep = io.DefineVariable<size_t>("step");
        adios2::Variable<double> varTimestamp = io.DefineVariable<double>("timestamp");
        
        // Open engine for writing
        adios2::Engine writer = io.Open("data-transfer", adios2::Mode::Write);
        
        if (rank == 0) {
            std::cout << "=== ADIOS2 Data Sender (Utah) ===" << std::endl;
            std::cout << "MPI Ranks: " << size << std::endl;
            std::cout << "Array size per rank: " << arraySize << " elements" << std::endl;
            std::cout << "Total data per step: " << (size * arraySize * sizeof(double)) / (1024.0 * 1024.0) << " MB" << std::endl;
            std::cout << "Number of steps: " << numSteps << std::endl;
            std::cout << "Starting data transmission..." << std::endl;
            std::cout << std::string(60, '=') << std::endl;
        }
        
        std::vector<double> data(arraySize);
        auto overallStart = std::chrono::high_resolution_clock::now();
        
        // Send data for each timestep
        for (size_t step = 0; step < numSteps; ++step) {
            auto stepStart = std::chrono::high_resolution_clock::now();
            
            // Generate data (simulate scientific computation)
            for (size_t i = 0; i < arraySize; ++i) {
                data[i] = rank * 1000.0 + step + static_cast<double>(i) / arraySize;
            }
            
            // Begin step
            writer.BeginStep();
            
            // Get timestamp
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            
            // Write data
            writer.Put(varData, data.data());
            if (rank == 0) {
                writer.Put(varStep, step);
                writer.Put(varTimestamp, timestamp);
            }
            
            // End step (this triggers the actual transfer)
            writer.EndStep();
            
            auto stepEnd = std::chrono::high_resolution_clock::now();
            auto stepDuration = std::chrono::duration<double>(stepEnd - stepStart).count();
            
            if (rank == 0) {
                double stepSizeMB = (size * arraySize * sizeof(double)) / (1024.0 * 1024.0);
                double throughputMBps = stepSizeMB / stepDuration;
                
                std::cout << "Step " << std::setw(3) << step 
                          << " | Time: " << std::fixed << std::setprecision(3) << std::setw(8) << stepDuration << " s"
                          << " | Size: " << std::setw(8) << std::setprecision(2) << stepSizeMB << " MB"
                          << " | Throughput: " << std::setw(8) << std::setprecision(2) << throughputMBps << " MB/s"
                          << std::endl;
            }
        }
        
        // Close writer
        writer.Close();
        
        auto overallEnd = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration<double>(overallEnd - overallStart).count();
        
        if (rank == 0) {
            std::cout << std::string(60, '=') << std::endl;
            double totalSizeMB = (numSteps * size * arraySize * sizeof(double)) / (1024.0 * 1024.0);
            double avgThroughputMBps = totalSizeMB / totalDuration;
            
            std::cout << "=== Transfer Complete ===" << std::endl;
            std::cout << "Total time: " << std::fixed << std::setprecision(3) << totalDuration << " seconds" << std::endl;
            std::cout << "Total data: " << std::setprecision(2) << totalSizeMB << " MB" << std::endl;
            std::cout << "Average throughput: " << std::setprecision(2) << avgThroughputMBps << " MB/s" << std::endl;
            std::cout << "Average throughput: " << std::setprecision(2) << avgThroughputMBps * 8.0 << " Mbps" << std::endl;
        }
        
    } catch (std::exception &e) {
        std::cerr << "Error on rank " << rank << ": " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    MPI_Finalize();
    return 0;
}
