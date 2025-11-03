# Quick Start: Two-Machine Transfer (Utah → Clemson)

## Prerequisites
- Build completed on both machines
- SSH access to both machines
- Network connectivity between sites

## Step-by-Step Instructions

### � **On Utah Machine (Sender) — run this FIRST**

```bash
# 1. Go to project root
cd /users/vlad777/research/adios2-transmission

# 2. Start the sender to create the contact file
./run_sender.sh

# Sender prints where it wrote the file, e.g.
#   /users/vlad777/research/build/data-transfer.sst
```

Leave the sender running until it finishes creating the contact file (you can Ctrl+C once the `.sst` file appears).

```bash
# 3. Copy the contact file to the receiver node
cd /users/vlad777/research/build
scp data-transfer.sst vlad777@node0.vlad777-277123.scidm-pg0.clemson.cloudlab.us:/users/vlad777/research/adios2-transmission/build/
```

Adjust the hostnames or paths as needed for your environment.

---

### � **On Clemson Machine (Receiver) — run this SECOND**

```bash
# 1. Log in to the receiver node
ssh vlad777@node0.vlad777-277123.scidm-pg0.clemson.cloudlab.us

# 2. Go to the project root
cd /users/vlad777/research/adios2-transmission

# 3. Ensure the copied contact file exists in the build directory
ls -la build/data-transfer.sst

# 4. Start the receiver, pointing at the same contact name
./run_receiver.sh
```

When the receiver starts, it reads the copied contact file and immediately connects to the sender.

---

## Manual Method (No Scripts)

### On Utah (Sender):
```bash
cd /users/vlad777/research/adios2-transmission/build
./sender
# Wait until the program creates data-transfer.sst (Ctrl+C if you only need the file)
```

### Copy the contact file:
```bash
scp data-transfer.sst vlad777@node0.vlad777-277123.scidm-pg0.clemson.cloudlab.us:/users/vlad777/research/adios2-transmission/build/
```

### On Clemson (Receiver):
```bash
cd /users/vlad777/research/adios2-transmission
ls -la build/data-transfer.sst   # verify the file was copied
./receiver data-transfer         # reads build/data-transfer.sst
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
- Make sure you ran the sender first and copied `data-transfer.sst`
- Verify the receiver is looking in the same directory (`build/` by default)

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
