# ADIOS2 SST Usage Guide

## Quick Start - Local Testing

To test both sender and receiver on the same machine:

```bash
./test_local.sh
```

This script will:
1. Start the sender (creates contact file)
2. Wait for contact file creation
3. Start the receiver (connects to sender)
4. Wait for completion and show results

---

## Distributed Setup (WAN Transfer)

### Understanding SST Flow

**IMPORTANT**: In ADIOS2 SST:
- The **SENDER (Writer)** creates the contact file
- The **RECEIVER (Reader)** uses that contact file to connect

### Step-by-Step Process

#### On Sender Machine (Utah):

1. **Start the sender first:**
   ```bash
   ./run_sender.sh
   ```

2. **Copy the contact file to receiver:**
   ```bash
   # The sender creates: data-transfer.sst
   scp data-transfer.sst user@receiver-host:/path/to/receiver/
   ```

#### On Receiver Machine (Clemson):

3. **Wait for contact file, then start receiver:**
   ```bash
   ./run_receiver.sh data-transfer.sst
   ```

---

## Manual Execution

### Sender:
```bash
cd build
mpirun -np 4 ./sender [contact-file-name]
# Default: data-transfer (creates data-transfer.sst)
```

### Receiver:
```bash
cd build
mpirun -np 4 ./receiver [contact-file-name]
# Must match sender's contact file name (without .sst extension)
```

---

## Common Issues

### "SstReader did not find active Writer contact info"

**Cause**: Receiver started before sender, or contact file doesn't exist.

**Solution**:
1. Always start sender first
2. Ensure contact file (.sst) is copied to receiver
3. Check that contact file path is correct

### "No such file or directory: ./receiver"

**Cause**: Running from wrong directory.

**Solution**: Use the wrapper scripts (`run_sender.sh`, `run_receiver.sh`) which handle paths correctly.

---

## File Locations

- **Executables**: `build/sender`, `build/receiver`
- **Contact file**: Created in current directory as `<name>.sst`
- **Logs**: `sender.log`, `receiver.log` (when using test_local.sh)
- **Metrics**: `transfer_metrics.csv` (created by receiver)
