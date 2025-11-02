# Network Configuration Guide for Utah → Clemson Transfer

## Understanding SST Connection Method

ADIOS2's SST engine uses a **contact file** to establish connections. This file contains:
- Receiver's IP address
- Port number(s)
- Connection metadata

### The contact file does NOT contain your data - only connection information!

---

## Setup Methods

### Method 1: Manual Contact File Transfer (Recommended for WAN)

This is the most common method for transfers between different sites like Utah and Clemson.

#### Step-by-Step Process:

**1. Start Receiver on Clemson:**
```bash
# On Clemson
cd /path/to/adios2-transmission/build
./receiver
```

Output will show:
```
=== ADIOS2 Data Receiver (Clemson) ===
Contact file: data-transfer.sst
MPI Ranks: 4
Waiting for data from sender...
```

**2. Contact File Created:**
The receiver creates `data-transfer.sst` in the current directory. This file contains Clemson's IP and port.

**3. Transfer Contact File to Utah:**

From your workstation:
```bash
# Copy from Clemson to your laptop
scp your-clemson-username@clemson-hostname:/path/to/build/data-transfer.sst .

# Copy from your laptop to Utah
scp data-transfer.sst your-utah-username@utah-hostname:/path/to/build/
```

Or directly (if you can SSH from Utah to Clemson):
```bash
# On Utah
scp your-clemson-username@clemson-hostname:/path/to/build/data-transfer.sst .
```

**4. Start Sender on Utah:**
```bash
# On Utah (make sure data-transfer.sst is in current directory)
cd /path/to/adios2-transmission/build
ls data-transfer.sst  # Verify file is present
./sender
```

The sender will:
- Read `data-transfer.sst`
- Extract Clemson's IP and port
- Connect directly to Clemson
- Begin data transfer

---

### Method 2: Shared Filesystem (If Available)

If both sites share a network filesystem (like a global parallel filesystem):

```bash
# Set environment variable on BOTH sites
export ADIOS2_SST_CONTACT_INFO_DIR=/shared/global/filesystem/path

# On Clemson (Receiver):
cd /path/to/build
./receiver
# Creates: /shared/global/filesystem/path/data-transfer.sst

# On Utah (Sender):
cd /path/to/build
./sender
# Reads: /shared/global/filesystem/path/data-transfer.sst
```

---

### Method 3: Custom Contact File Location

You can now specify a custom contact file name/location:

**On Clemson (Receiver):**
```bash
./receiver my-transfer
# Creates: my-transfer.sst
```

**On Utah (Sender):**
```bash
# After copying my-transfer.sst to current directory
./sender my-transfer
# Reads: my-transfer.sst
```

Or use full paths via environment variable:
```bash
# On both sites
export ADIOS2_SST_CONTACT_INFO=/specific/path/to/contact-file.sst

./receiver
./sender
```

---

## Network Requirements

### Firewall Configuration

**On Clemson (Receiver side):**
- Must allow **INCOMING** connections on high-numbered ports (50000-65535)
- SST dynamically chooses ports in this range

**On Utah (Sender side):**
- Must allow **OUTGOING** connections to Clemson's IP
- No special incoming port requirements

### Testing Network Connectivity

Before running the transfer, test basic connectivity:

```bash
# From Utah, test connection to Clemson
ping clemson-hostname
telnet clemson-hostname 22  # Test general connectivity

# Check if you can see Clemson's hostname/IP
nslookup clemson-hostname
```

### Common Network Issues

**Problem**: "Connection timeout" or "Cannot connect"

**Solutions**:
1. Check firewall rules on both sides
2. Verify the contact file contains the correct IP (open it with a text editor)
3. Ensure you're using the receiver's **public/external** IP if crossing networks
4. Check if IT has blocked high-numbered ports

**Problem**: Contact file contains wrong IP address

**Solution**: On Clemson, you may need to set the network interface:
```bash
# Find your external network interface
ip addr show

# Set which interface SST should use
export ADIOS2_SST_INTERFACE=eth0  # or ib0, eno1, etc.
./receiver
```

---

## Example Workflow

### Complete Transfer Session

**Terminal 1 - Clemson (Receiver):**
```bash
$ ssh clemson-user@clemson.hpc.edu
$ cd ~/adios2-transmission/build
$ mpirun -np 4 ./receiver
=== ADIOS2 Data Receiver (Clemson) ===
Contact file: data-transfer.sst
MPI Ranks: 4
Waiting for data from sender...
[Waiting for connection...]
```

**Terminal 2 - Your Laptop (Transfer contact file):**
```bash
$ scp clemson-user@clemson.hpc.edu:~/adios2-transmission/build/data-transfer.sst .
data-transfer.sst                         100%  256     2.1KB/s   00:00

$ scp data-transfer.sst utah-user@utah.hpc.edu:~/adios2-transmission/build/
data-transfer.sst                         100%  256     2.1KB/s   00:00
```

**Terminal 3 - Utah (Sender):**
```bash
$ ssh utah-user@utah.hpc.edu
$ cd ~/adios2-transmission/build
$ ls -l data-transfer.sst  # Verify file is present
-rw-r--r-- 1 utah-user group 256 Nov  2 10:15 data-transfer.sst

$ mpirun -np 4 ./sender
=== ADIOS2 Data Sender (Utah) ===
Contact file: data-transfer.sst
MPI Ranks: 4
Array size per rank: 10000000 elements
Total data per step: 320.00 MB
Number of steps: 10
Starting data transmission...
============================================================
Step   0 | Time:    2.341 s | Size:   320.00 MB | Throughput:   136.72 MB/s
Step   1 | Time:    2.289 s | Size:   320.00 MB | Throughput:   139.80 MB/s
...
```

**Terminal 1 - Clemson (You'll see):**
```
Step   0 | Time:    2.345 s | Size:   320.00 MB | Throughput:   136.48 MB/s
Step   1 | Time:    2.292 s | Size:   320.00 MB | Throughput:   139.60 MB/s
...
```

---

## Advanced: Using IP Address Directly

If you know Clemson's IP address, you can verify the contact file contains it:

```bash
# On Clemson after starting receiver
cat data-transfer.sst

# You should see something like:
# CM:1.0:192.168.1.100:54321:...
#           ^^^^^^^^^^^^^^^ This is Clemson's IP
```

The IP in the contact file should be:
- ✅ Clemson's **public/external** IP (if crossing networks)
- ✅ Clemson's **internal** IP (if on same network/VPN)
- ❌ NOT `127.0.0.1` or `localhost`

---

## Troubleshooting Checklist

- [ ] Receiver started BEFORE sender
- [ ] Contact file exists and is readable
- [ ] Contact file is in sender's current directory (or path set via env var)
- [ ] Firewall allows incoming connections on Clemson
- [ ] Firewall allows outgoing connections from Utah
- [ ] Network path exists between sites (can ping)
- [ ] Using correct IP address (not localhost)
- [ ] Same contact file name used on both sides
- [ ] MPI process counts match (or at least compatible)

---

## Environment Variables Reference

```bash
# Specify contact file directory
export ADIOS2_SST_CONTACT_INFO_DIR=/path/to/directory

# Specify exact contact file path
export ADIOS2_SST_CONTACT_INFO=/path/to/file.sst

# Specify network interface (receiver side)
export ADIOS2_SST_INTERFACE=eth0

# Increase timeout (in seconds, default is 60)
export ADIOS2_SST_TIMEOUT=300
```

---

## Quick Reference

| Task | Command |
|------|---------|
| Start receiver | `./receiver [name]` |
| Start sender | `./sender [name]` |
| Copy contact file | `scp user@host:path/file.sst .` |
| Check contact file | `cat data-transfer.sst` |
| Set contact dir | `export ADIOS2_SST_CONTACT_INFO_DIR=/path` |
| Set timeout | `export ADIOS2_SST_TIMEOUT=300` |
| Check firewall | `sudo ufw status` (Ubuntu) |
| Test connectivity | `ping hostname` |

---

**Remember**: The receiver must start first and stay running while you transfer the contact file and start the sender!
