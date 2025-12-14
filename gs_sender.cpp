/*
 * Gray-Scott Simulation with Live ADIOS2 SST Streaming
 * 
 * Distributed 3D reaction-diffusion simulation that generates data
 * in real-time and streams each output step via ADIOS2 SST.
 */

#include <adios2.h>
#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <thread>
#include <algorithm>

// Gray-Scott parameters
struct GSParams {
    double Du = 0.2;      // Diffusion rate for U
    double Dv = 0.1;      // Diffusion rate for V
    double F  = 0.04;     // Feed rate
    double k  = 0.06;     // Kill rate
    double dt = 1.0;      // Time step
    double dx = 1.0;      // Grid spacing
};

class GrayScottSimulation {
public:
    GrayScottSimulation(int rank, int size, 
                        size_t globalNz, size_t globalNy, size_t globalNx,
                        const GSParams& params)
        : rank_(rank), size_(size), 
          globalNz_(globalNz), globalNy_(globalNy), globalNx_(globalNx),
          params_(params)
    {
        // 1D decomposition along Z axis
        size_t baseSlices = globalNz_ / size_;
        size_t remainder = globalNz_ % size_;
        
        localNz_ = baseSlices + (static_cast<size_t>(rank_) < remainder ? 1 : 0);
        zStart_ = rank_ * baseSlices + std::min(static_cast<size_t>(rank_), remainder);
        
        localNy_ = globalNy_;
        localNx_ = globalNx_;
        
        // Allocate with ghost layers (1 cell on each side of Z)
        size_t totalSize = (localNz_ + 2) * localNy_ * localNx_;
        U_.resize(totalSize, 1.0);  // U starts at 1
        V_.resize(totalSize, 0.0);  // V starts at 0
        U_new_.resize(totalSize);
        V_new_.resize(totalSize);
        
        // Seed initial perturbation in center of global domain
        seedInitialCondition();
        
        // Determine neighbors for halo exchange
        rankBelow_ = (rank_ > 0) ? rank_ - 1 : MPI_PROC_NULL;
        rankAbove_ = (rank_ < size_ - 1) ? rank_ + 1 : MPI_PROC_NULL;
        
        // For periodic BCs (optional - currently using fixed boundaries)
        // rankBelow_ = (rank_ > 0) ? rank_ - 1 : size_ - 1;
        // rankAbove_ = (rank_ < size_ - 1) ? rank_ + 1 : 0;
    }
    
    void seedInitialCondition() {
        // Seed a cube of V=0.25, U=0.5 in the center of the domain
        size_t centerZ = globalNz_ / 2;
        size_t centerY = globalNy_ / 2;
        size_t centerX = globalNx_ / 2;
        size_t seedRadius = std::min({globalNz_, globalNy_, globalNx_}) / 10;
        
        for (size_t lz = 0; lz < localNz_; ++lz) {
            size_t gz = zStart_ + lz;
            for (size_t ly = 0; ly < localNy_; ++ly) {
                for (size_t lx = 0; lx < localNx_; ++lx) {
                    // Check if within seed region
                    if (std::abs(static_cast<int>(gz) - static_cast<int>(centerZ)) <= static_cast<int>(seedRadius) &&
                        std::abs(static_cast<int>(ly) - static_cast<int>(centerY)) <= static_cast<int>(seedRadius) &&
                        std::abs(static_cast<int>(lx) - static_cast<int>(centerX)) <= static_cast<int>(seedRadius)) {
                        size_t idx = index(lz + 1, ly, lx); // +1 for ghost layer
                        U_[idx] = 0.5;
                        V_[idx] = 0.25;
                    }
                }
            }
        }
    }
    
    void exchangeHalos() {
        size_t sliceSize = localNy_ * localNx_;
        std::vector<double> sendBufDown(sliceSize), recvBufDown(sliceSize);
        std::vector<double> sendBufUp(sliceSize), recvBufUp(sliceSize);
        std::vector<double> sendBufDownV(sliceSize), recvBufDownV(sliceSize);
        std::vector<double> sendBufUpV(sliceSize), recvBufUpV(sliceSize);
        
        // Pack bottom slice (z=1, first real layer) to send down
        // Pack top slice (z=localNz_, last real layer) to send up
        for (size_t ly = 0; ly < localNy_; ++ly) {
            for (size_t lx = 0; lx < localNx_; ++lx) {
                size_t sIdx = ly * localNx_ + lx;
                sendBufDown[sIdx] = U_[index(1, ly, lx)];
                sendBufUp[sIdx] = U_[index(localNz_, ly, lx)];
                sendBufDownV[sIdx] = V_[index(1, ly, lx)];
                sendBufUpV[sIdx] = V_[index(localNz_, ly, lx)];
            }
        }
        
        // Exchange U halos
        MPI_Sendrecv(sendBufDown.data(), sliceSize, MPI_DOUBLE, rankBelow_, 0,
                     recvBufUp.data(), sliceSize, MPI_DOUBLE, rankAbove_, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Sendrecv(sendBufUp.data(), sliceSize, MPI_DOUBLE, rankAbove_, 1,
                     recvBufDown.data(), sliceSize, MPI_DOUBLE, rankBelow_, 1,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Exchange V halos
        MPI_Sendrecv(sendBufDownV.data(), sliceSize, MPI_DOUBLE, rankBelow_, 2,
                     recvBufUpV.data(), sliceSize, MPI_DOUBLE, rankAbove_, 2,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Sendrecv(sendBufUpV.data(), sliceSize, MPI_DOUBLE, rankAbove_, 3,
                     recvBufDownV.data(), sliceSize, MPI_DOUBLE, rankBelow_, 3,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Unpack into ghost layers
        for (size_t ly = 0; ly < localNy_; ++ly) {
            for (size_t lx = 0; lx < localNx_; ++lx) {
                size_t sIdx = ly * localNx_ + lx;
                if (rankAbove_ != MPI_PROC_NULL) {
                    U_[index(localNz_ + 1, ly, lx)] = recvBufUp[sIdx];
                    V_[index(localNz_ + 1, ly, lx)] = recvBufUpV[sIdx];
                }
                if (rankBelow_ != MPI_PROC_NULL) {
                    U_[index(0, ly, lx)] = recvBufDown[sIdx];
                    V_[index(0, ly, lx)] = recvBufDownV[sIdx];
                }
            }
        }
        
        // Fixed boundary conditions at domain edges
        if (rankBelow_ == MPI_PROC_NULL) {
            for (size_t ly = 0; ly < localNy_; ++ly) {
                for (size_t lx = 0; lx < localNx_; ++lx) {
                    U_[index(0, ly, lx)] = U_[index(1, ly, lx)];
                    V_[index(0, ly, lx)] = V_[index(1, ly, lx)];
                }
            }
        }
        if (rankAbove_ == MPI_PROC_NULL) {
            for (size_t ly = 0; ly < localNy_; ++ly) {
                for (size_t lx = 0; lx < localNx_; ++lx) {
                    U_[index(localNz_ + 1, ly, lx)] = U_[index(localNz_, ly, lx)];
                    V_[index(localNz_ + 1, ly, lx)] = V_[index(localNz_, ly, lx)];
                }
            }
        }
    }
    
    void step() {
        exchangeHalos();
        
        double dx2 = params_.dx * params_.dx;
        double dt = params_.dt;
        double Du = params_.Du;
        double Dv = params_.Dv;
        double F = params_.F;
        double k = params_.k;
        
        // Update interior points
        for (size_t lz = 1; lz <= localNz_; ++lz) {
            for (size_t ly = 0; ly < localNy_; ++ly) {
                for (size_t lx = 0; lx < localNx_; ++lx) {
                    size_t idx = index(lz, ly, lx);
                    
                    double u = U_[idx];
                    double v = V_[idx];
                    
                    // 7-point stencil Laplacian
                    double laplacianU = 0.0;
                    double laplacianV = 0.0;
                    
                    // Z direction
                    laplacianU += U_[index(lz - 1, ly, lx)] + U_[index(lz + 1, ly, lx)];
                    laplacianV += V_[index(lz - 1, ly, lx)] + V_[index(lz + 1, ly, lx)];
                    
                    // Y direction (periodic)
                    size_t lyM = (ly > 0) ? ly - 1 : localNy_ - 1;
                    size_t lyP = (ly < localNy_ - 1) ? ly + 1 : 0;
                    laplacianU += U_[index(lz, lyM, lx)] + U_[index(lz, lyP, lx)];
                    laplacianV += V_[index(lz, lyM, lx)] + V_[index(lz, lyP, lx)];
                    
                    // X direction (periodic)
                    size_t lxM = (lx > 0) ? lx - 1 : localNx_ - 1;
                    size_t lxP = (lx < localNx_ - 1) ? lx + 1 : 0;
                    laplacianU += U_[index(lz, ly, lxM)] + U_[index(lz, ly, lxP)];
                    laplacianV += V_[index(lz, ly, lxM)] + V_[index(lz, ly, lxP)];
                    
                    laplacianU = (laplacianU - 6.0 * u) / dx2;
                    laplacianV = (laplacianV - 6.0 * v) / dx2;
                    
                    // Gray-Scott reaction terms
                    double uvv = u * v * v;
                    double dudt = Du * laplacianU - uvv + F * (1.0 - u);
                    double dvdt = Dv * laplacianV + uvv - (F + k) * v;
                    
                    U_new_[idx] = u + dt * dudt;
                    V_new_[idx] = v + dt * dvdt;
                    
                    // Clamp values to [0, 1]
                    U_new_[idx] = std::max(0.0, std::min(1.0, U_new_[idx]));
                    V_new_[idx] = std::max(0.0, std::min(1.0, V_new_[idx]));
                }
            }
        }
        
        // Swap buffers
        std::swap(U_, U_new_);
        std::swap(V_, V_new_);
    }
    
    // Get data without ghost layers for output
    std::vector<double> getU() const {
        std::vector<double> result(localNz_ * localNy_ * localNx_);
        for (size_t lz = 0; lz < localNz_; ++lz) {
            for (size_t ly = 0; ly < localNy_; ++ly) {
                for (size_t lx = 0; lx < localNx_; ++lx) {
                    result[lz * localNy_ * localNx_ + ly * localNx_ + lx] = 
                        U_[index(lz + 1, ly, lx)];
                }
            }
        }
        return result;
    }
    
    std::vector<double> getV() const {
        std::vector<double> result(localNz_ * localNy_ * localNx_);
        for (size_t lz = 0; lz < localNz_; ++lz) {
            for (size_t ly = 0; ly < localNy_; ++ly) {
                for (size_t lx = 0; lx < localNx_; ++lx) {
                    result[lz * localNy_ * localNx_ + ly * localNx_ + lx] = 
                        V_[index(lz + 1, ly, lx)];
                }
            }
        }
        return result;
    }
    
    size_t getLocalNz() const { return localNz_; }
    size_t getLocalNy() const { return localNy_; }
    size_t getLocalNx() const { return localNx_; }
    size_t getZStart() const { return zStart_; }
    size_t getGlobalNz() const { return globalNz_; }
    size_t getGlobalNy() const { return globalNy_; }
    size_t getGlobalNx() const { return globalNx_; }
    
private:
    inline size_t index(size_t lz, size_t ly, size_t lx) const {
        // lz includes ghost layer offset (0 = bottom ghost, 1..localNz_ = real, localNz_+1 = top ghost)
        return lz * localNy_ * localNx_ + ly * localNx_ + lx;
    }
    
    int rank_, size_;
    size_t globalNz_, globalNy_, globalNx_;
    size_t localNz_, localNy_, localNx_;
    size_t zStart_;
    int rankBelow_, rankAbove_;
    GSParams params_;
    
    std::vector<double> U_, V_;
    std::vector<double> U_new_, V_new_;
};


int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Default parameters
    size_t gridSize = 128;       // Global grid size (cubic)
    int totalSteps = 10000;      // Total simulation steps
    int outputInterval = 100;    // Output every N steps
    std::string contactFile = "gs-simulation";
    
    // Parse command line
    if (argc > 1) gridSize = std::stoul(argv[1]);
    if (argc > 2) totalSteps = std::stoi(argv[2]);
    if (argc > 3) outputInterval = std::stoi(argv[3]);
    if (argc > 4) contactFile = argv[4];
    
    // Gray-Scott parameters (coral pattern)
    GSParams params;
    params.F = 0.0545;
    params.k = 0.062;
    params.Du = 0.2;
    params.Dv = 0.1;
    params.dt = 1.0;
    params.dx = 1.0;
    
    if (rank == 0) {
        std::cout << "=== Gray-Scott Simulation with SST Streaming ===" << std::endl;
        std::cout << "Grid size: " << gridSize << " x " << gridSize << " x " << gridSize << std::endl;
        std::cout << "Total steps: " << totalSteps << std::endl;
        std::cout << "Output interval: " << outputInterval << " steps" << std::endl;
        std::cout << "MPI ranks: " << size << std::endl;
        std::cout << "Parameters: F=" << params.F << ", k=" << params.k << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    // Initialize simulation
    GrayScottSimulation sim(rank, size, gridSize, gridSize, gridSize, params);
    
    // Initialize ADIOS2
    adios2::ADIOS adios(MPI_COMM_WORLD);
    adios2::IO io = adios.DeclareIO("GrayScottIO");
    io.SetEngine("SST");
    io.SetParameters({
        {"RendezvousReaderCount", "1"},
        {"QueueLimit", "5"},
        {"QueueFullPolicy", "Block"},
        {"ControlTransport", "sockets"},
        {"DataTransport", "sockets"},
        {"OpenTimeoutSecs", "300"},
        {"MarshalMethod", "BP5"}
    });
    
    // Define variables
    adios2::Variable<double> varU = io.DefineVariable<double>(
        "U",
        {sim.getGlobalNz(), sim.getGlobalNy(), sim.getGlobalNx()},
        {sim.getZStart(), 0, 0},
        {sim.getLocalNz(), sim.getLocalNy(), sim.getLocalNx()}
    );
    
    adios2::Variable<double> varV = io.DefineVariable<double>(
        "V",
        {sim.getGlobalNz(), sim.getGlobalNy(), sim.getGlobalNx()},
        {sim.getZStart(), 0, 0},
        {sim.getLocalNz(), sim.getLocalNy(), sim.getLocalNx()}
    );
    
    adios2::Variable<int32_t> varStep;
    if (rank == 0) {
        varStep = io.DefineVariable<int32_t>("step");
    }
    
    // Start SST monitor thread to display connection string
    std::thread sstMonitor;
    if (rank == 0) {
        sstMonitor = std::thread([contactFile]() {
            std::string sstFileName = contactFile + ".sst";
            for (int i = 0; i < 30; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::ifstream sstFile(sstFileName);
                if (sstFile.is_open()) {
                    std::string line;
                    std::getline(sstFile, line);
                    std::getline(sstFile, line);
                    sstFile.close();
                    
                    if (!line.empty()) {
                        std::cout << "\n*** SST CONNECTION STRING ***" << std::endl;
                        std::cout << line << std::endl;
                        std::cout << "\nRun this on receiver machine (choose your MPI ranks):" << std::endl;
                        std::cout << "  mpirun -np <num_ranks> ./receiver \"" << line << "\"" << std::endl;
                        std::cout << std::string(60, '=') << std::endl;
                        std::cout << "\nWaiting for receiver to connect..." << std::endl;
                        break;
                    }
                }
            }
        });
    }
    
    // Open SST writer
    adios2::Engine writer = io.Open(contactFile, adios2::Mode::Write);
    
    if (rank == 0 && sstMonitor.joinable()) {
        sstMonitor.join();
    }
    
    auto overallStart = std::chrono::high_resolution_clock::now();
    int outputCount = 0;
    
    // Main simulation loop
    for (int step = 0; step <= totalSteps; ++step) {
        // Output at interval
        if (step % outputInterval == 0) {
            auto stepStart = std::chrono::high_resolution_clock::now();
            
            writer.BeginStep();
            
            auto dataU = sim.getU();
            auto dataV = sim.getV();
            
            writer.Put(varU, dataU.data());
            writer.Put(varV, dataV.data());
            
            if (rank == 0) {
                int32_t stepVal = step;
                writer.Put(varStep, stepVal);
            }
            
            writer.EndStep();
            
            auto stepEnd = std::chrono::high_resolution_clock::now();
            double stepTime = std::chrono::duration<double>(stepEnd - stepStart).count();
            
            double localDataMB = (dataU.size() + dataV.size()) * sizeof(double) / (1024.0 * 1024.0);
            double globalDataMB = 0.0;
            MPI_Reduce(&localDataMB, &globalDataMB, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            
            if (rank == 0) {
                std::cout << "Output " << std::setw(4) << outputCount 
                          << " (sim step " << std::setw(6) << step << ")"
                          << " | Time: " << std::fixed << std::setprecision(3) << stepTime << " s"
                          << " | Size: " << std::setprecision(2) << globalDataMB << " MB"
                          << " | Throughput: " << std::setprecision(2) << globalDataMB / stepTime << " MB/s"
                          << std::endl;
            }
            
            outputCount++;
        }
        
        // Advance simulation (except on last iteration)
        if (step < totalSteps) {
            sim.step();
        }
    }
    
    writer.Close();
    
    auto overallEnd = std::chrono::high_resolution_clock::now();
    double totalTime = std::chrono::duration<double>(overallEnd - overallStart).count();
    
    if (rank == 0) {
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "=== Simulation Complete ===" << std::endl;
        std::cout << "Total simulation steps: " << totalSteps << std::endl;
        std::cout << "Total output steps: " << outputCount << std::endl;
        std::cout << "Total time: " << std::fixed << std::setprecision(3) << totalTime << " s" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    MPI_Finalize();
    return 0;
}
