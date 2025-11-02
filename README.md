# ADIOS2 WAN Data Transfer: Utah → Clemson

This project enables high-performance data transfer from Utah to Clemson using ADIOS2's SST (Sustainable Staging Transport) engine with built-in performance monitoring.

## Overview

- **Sender (Utah)**: Generates and sends data using ADIOS2
- **Receiver (Clemson)**: Receives data and measures transmission time and throughput
- **Engine**: SST (Sustainable Staging Transport) optimized for WAN transfers
- **Features**: 
  - Real-time throughput measurement
  - Latency tracking
  - Detailed performance metrics export
  - MPI-parallel data transfer

## Prerequisites

### Both Sites (Utah and Clemson)

1. **MPI** (OpenMPI, MPICH, or Intel MPI)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libopenmpi-dev openmpi-bin
   
   # CentOS/RHEL
   sudo yum install openmpi openmpi-devel
   
   # Load MPI module (on HPC systems)
   module load openmpi
   ```

2. **ADIOS2** (version 2.7+)
   ```bash
   # Option 1: Install from source
   git clone https://github.com/ornladios/ADIOS2.git
   cd ADIOS2
   mkdir build && cd build
   cmake -DCMAKE_INSTALL_PREFIX=/path/to/install \
         -DADIOS2_USE_MPI=ON \
         -DADIOS2_USE_SST=ON \
         ..
   make -j8
   sudo make install
   
   # Option 2: Use system package manager
   sudo apt-get install libadios2-dev  # Ubuntu 20.04+
   ```

3. **CMake** (version 3.12+)
   ```bash
   sudo apt-get install cmake  # Ubuntu/Debian
   sudo yum install cmake      # CentOS/RHEL
   ```

## Building the Project

### On Utah (Sender Side)

```bash
cd /users/vlad777/research
mkdir build && cd build
cmake ..
make
```

This will create two executables in `build/bin/`:
- `sender` - Sends data from Utah
- `receiver` - Receives data at Clemson

### On Clemson (Receiver Side)

Copy the entire project to Clemson and build:

```bash
# Copy project files to Clemson
scp -r /users/vlad777/research user@clemson-host:/path/to/clemson/directory

# On Clemson machine
cd /path/to/clemson/directory
mkdir build && cd build
cmake ..
make
```

## Network Configuration

### Firewall and Port Settings

ADIOS2 SST uses dynamic port allocation. Ensure the following:

1. **On Utah (Sender)**:
   - Outbound connections allowed on high-numbered ports (typically 50000-65535)
   
2. **On Clemson (Receiver)**:
   - Inbound connections allowed on high-numbered ports
   - If behind a firewall, you may need to open specific port ranges

### Contact Point File

SST uses a contact file for establishing connections. The receiver creates this file, and the sender reads it.

**Option 1**: Shared filesystem (if available)
- Both sites mount the same network filesystem
- Contact file is automatically shared

**Option 2**: Manual file transfer
- Receiver creates contact file
- Copy file to sender location before sender starts

**Option 3**: Environment variable
```bash
export ADIOS2_SST_CONTACT_INFO_FILE=/shared/path/contact.info
```

## Running the Transfer

### Step 1: Start Receiver (Clemson)

The receiver must start first and wait for the sender:

```bash
# Single process
cd build/bin
./receiver

# MPI parallel (recommended for large data)
mpirun -np 4 ./receiver
```

The receiver will:
- Create a contact file (e.g., `data-transfer.sst`)
- Wait for incoming connection
- Display "Waiting for data from sender..."

### Step 2: Share Contact Information

**If using shared filesystem**: No action needed.

**If using manual transfer**:
```bash
# On Clemson, the contact file is created in the working directory
# Copy it to Utah
scp data-transfer.sst user@utah-host:/path/to/sender/directory/
```

### Step 3: Start Sender (Utah)

```bash
# Make sure you're in the same directory as the contact file
cd build/bin

# Single process
./sender

# MPI parallel (match receiver process count)
mpirun -np 4 ./sender
```

## Configuration Parameters

### Data Transfer Settings

Edit `sender.cpp` to adjust:

```cpp
const size_t arraySize = 10000000;  // Elements per rank per step
const size_t numSteps = 10;          // Number of timesteps
```

Current defaults:
- **Data per step**: ~40 MB per rank
- **Number of steps**: 10
- **Total data** (4 ranks, 10 steps): ~1.6 GB

### ADIOS2 Engine Parameters

For advanced tuning, edit the XML configuration files:

**Sender** (`adios2_config_sender.xml`):
- `QueueLimit`: Buffer size for outgoing data
- `MarshalMethod`: Data serialization format

**Receiver** (`adios2_config_receiver.xml`):
- `OpenTimeoutSecs`: Connection timeout (increase for slow networks)

To use XML config files:
```cpp
// In sender.cpp or receiver.cpp
adios2::ADIOS adios("adios2_config_sender.xml", MPI_COMM_WORLD);
```

## Output and Metrics

### Console Output

**Sender Output**:
```
=== ADIOS2 Data Sender (Utah) ===
MPI Ranks: 4
Array size per rank: 10000000 elements
Total data per step: 320.00 MB
Number of steps: 10
============================================================
Step   0 | Time:    2.341 s | Size:   320.00 MB | Throughput:   136.72 MB/s
Step   1 | Time:    2.289 s | Size:   320.00 MB | Throughput:   139.80 MB/s
...
```

**Receiver Output**:
```
=== ADIOS2 Data Receiver (Clemson) ===
MPI Ranks: 4
Waiting for data from sender...
============================================================
Step   0 | Time:    2.345 s | Size:   320.00 MB | Throughput:   136.48 MB/s | Latency:    2.350 s
Step   1 | Time:    2.292 s | Size:   320.00 MB | Throughput:   139.60 MB/s | Latency:    2.295 s
...
```

### Metrics File

The receiver automatically generates `transfer_metrics.csv`:

```csv
Step,Time(s),Size(MB),Throughput(MB/s),Throughput(Mbps)
0,2.345123,320.00,136.48,1091.84
1,2.292456,320.00,139.60,1116.80
...
```

Use this file for:
- Plotting throughput over time
- Statistical analysis
- Performance optimization

## Troubleshooting

### Connection Issues

**Problem**: Receiver times out waiting for sender
- **Solution**: Increase `OpenTimeoutSecs` in receiver XML config
- **Check**: Firewall settings on both sides
- **Verify**: Contact file is accessible to sender

**Problem**: "Cannot find contact file"
- **Solution**: Ensure contact file is in sender's working directory
- **Check**: File permissions

### Performance Issues

**Low throughput**:
1. Check network bandwidth: `iperf3 -c <receiver-host>`
2. Increase `QueueLimit` in sender config
3. Use more MPI ranks for parallel transfer
4. Enable compression in XML config

**High latency**:
1. Verify network path: `traceroute <receiver-host>`
2. Check for packet loss: `ping -c 100 <receiver-host>`
3. Consider using dedicated research network (if available)

### Build Issues

**ADIOS2 not found**:
```bash
cmake -DADIOS2_DIR=/path/to/adios2/install/lib/cmake/adios2 ..
```

**MPI not found**:
```bash
export PATH=/path/to/mpi/bin:$PATH
cmake ..
```

## Advanced Usage

### Using Compression

Edit sender.cpp to enable compression:

```cpp
// After DeclareIO
io.SetEngine("SST");

// Define compression operator
adios2::Operator compressor = adios.DefineOperator("ZFPCompressor", "zfp");

// Add compression to variable
varData.AddOperation(compressor, {{"accuracy", "0.001"}});
```

### Different Transport Methods

Change engine type for different scenarios:

- **SST** (default): Best for WAN, real-time streaming
- **DataMan**: Alternative WAN transport with WAN mode
- **BP4/BP5**: File-based transfer (for intermittent connectivity)

```cpp
io.SetEngine("DataMan");
io.SetParameters({{"Transport", "WAN"}});
```

### Custom Data Sizes

Adjust for your specific data:

```cpp
// Large scientific datasets
const size_t arraySize = 100000000;  // 100M elements = ~800 MB
const size_t numSteps = 100;

// Small test transfers
const size_t arraySize = 1000000;    // 1M elements = ~8 MB
const size_t numSteps = 5;
```

## Performance Optimization Tips

1. **MPI Parallelism**: Use multiple ranks to saturate network bandwidth
2. **Queue Size**: Increase `QueueLimit` for bursty data
3. **Compression**: Enable for compressible scientific data
4. **Network**: Use dedicated research networks (e.g., Internet2, ESnet)
5. **Buffer Size**: Tune TCP buffer sizes on both systems:
   ```bash
   # Increase TCP buffer sizes
   sudo sysctl -w net.core.rmem_max=134217728
   sudo sysctl -w net.core.wmem_max=134217728
   ```

## Example Session

```bash
# ===== On Clemson (Receiver) =====
cd /path/to/research/build/bin
mpirun -np 4 ./receiver
# Wait for "Waiting for data from sender..." message

# ===== On Utah (Sender) =====
# (Ensure contact file is present if not using shared filesystem)
cd /path/to/research/build/bin
mpirun -np 4 ./sender

# ===== Expected Output =====
# Both sides will show real-time progress
# Receiver creates transfer_metrics.csv with detailed statistics
```

## References

- [ADIOS2 Documentation](https://adios2.readthedocs.io/)
- [SST Engine Guide](https://adios2.readthedocs.io/en/latest/engines/engines.html#sst-sustainable-staging-transport)
- [ADIOS2 GitHub](https://github.com/ornladios/ADIOS2)

## License

This project uses ADIOS2, which is licensed under the Apache License 2.0.
