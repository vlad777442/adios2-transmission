# Quick Start: Two-Machine Transfer (Utah → Clemson)

## Prerequisites
- Build completed on both machines
- SSH access to both machines
- Network connectivity between sites

## Step-by-Step Instructions

### 🔵 **On Clemson Machine (Receiver)**

```bash
# 1. Go to build directory
cd /path/to/adios2-transmission/build

# 2. Start receiver (it will wait for sender)
../run_receiver.sh

# Output will show:
#   Contact file will be: /path/to/build/data-transfer.sst
#   On sender machine (Utah), run:
#     scp clemson-host:/path/to/build/data-transfer.sst ...
```

**Leave this terminal running!** The receiver is now waiting.

---

### 🔴 **On Utah Machine (Sender)**

Open a NEW terminal/session:

```bash
# 1. Copy contact file from Clemson
scp user@clemson-host:/path/to/build/data-transfer.sst ./

# Example with real hostnames:
# scp vlad@palmetto.clemson.edu:/home/vlad/research/build/data-transfer.sst ./

# 2. Verify file was copied
ls -la data-transfer.sst

# 3. Start sender
../run_sender.sh data-transfer.sst
```

The transfer will begin immediately!

---

## Alternative: One Command from Utah

If you prefer, you can do everything from Utah:

```bash
# Terminal 1: Start receiver on Clemson via SSH
ssh user@clemson-host 'cd /path/to/build && ../run_receiver.sh' &

# Wait a few seconds for receiver to start
sleep 5

# Terminal 2: Copy contact file and run sender
scp user@clemson-host:/path/to/build/data-transfer.sst ./
../run_sender.sh data-transfer.sst
```

---

## Manual Method (No Scripts)

### On Clemson:
```bash
cd build
export ADIOS2_SST_CONTACT_INFO_FILE=$PWD/data-transfer.sst
./receiver
```

### On Utah:
```bash
# Copy contact file
scp clemson:/path/to/build/data-transfer.sst ./

cd build
export ADIOS2_SST_CONTACT_INFO_FILE=$PWD/data-transfer.sst
./sender
```

---

## Firewall Notes

SST uses TCP sockets. Ensure:
- **Clemson** allows incoming connections (receiver listens)
- **Utah** allows outgoing connections
- Ports: SST uses dynamic ports (ephemeral range, typically 49152-65535)

If behind strict firewall, you may need to:
1. Open port range on Clemson
2. Or use SSH tunneling (see NETWORK_SETUP.md)

---

## Troubleshooting

**"Contact file not found"**
- Make sure receiver is running first
- Check the path is correct when copying with scp

**"Connection timeout"**
- Check network connectivity: `ping clemson-host`
- Check firewall allows connections
- Increase timeout in code if needed

**"Cannot connect to receiver"**
- Verify both machines can reach each other
- Try running both on same machine first to verify code works
- Check contact file contains correct IP address:
  ```bash
  cat data-transfer.sst
  ```

---

## Expected Output

**Receiver (Clemson):**
```
=== ADIOS2 Data Receiver (Clemson) ===
MPI Ranks: 1
Waiting for data from sender...
============================================================
Step   0 | Time:    2.345 s | Size:   76.29 MB | Throughput:   32.50 MB/s
...
=== Reception Complete ===
Total data received: 762.94 MB
Average throughput: 28.45 MB/s
```

**Sender (Utah):**
```
=== ADIOS2 Data Sender (Utah) ===
MPI Ranks: 1
Starting data transmission...
============================================================
Step   0 | Time:    2.341 s | Size:   76.29 MB | Throughput:   32.60 MB/s
...
=== Transfer Complete ===
Average throughput: 28.50 MB/s
```

The receiver will also create `transfer_metrics.csv` with detailed performance data.
