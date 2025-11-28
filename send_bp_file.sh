#!/bin/bash
# send_bp_file.sh - Send BP file to remote receiver

echo "=== ADIOS2 BP File Relay Sender ==="
echo ""

# Default values
BP_FILE="${1:-/users/vlad777/research/gs-2gb.bp}"
CONTACT_NAME="${2:-data-transfer}"
NUM_RANKS="${3:-4}"

# Verify BP file exists
if [ ! -d "$BP_FILE" ]; then
    echo "Error: BP file directory not found: $BP_FILE"
    echo ""
    echo "Usage: $0 <path_to_bp_file> [contact_name] [num_mpi_ranks]"
    echo "Example: $0 /users/vlad777/research/gs-2gb.bp data-transfer 4"
    exit 1
fi

echo "BP file: $BP_FILE"
echo "Contact name: $CONTACT_NAME"
echo "MPI ranks: $NUM_RANKS"
echo ""
echo "Make sure the receiver is running first!"
echo ""
echo "Starting sender..."
echo "============================================================"
echo ""

# Run sender with MPI
cd "$(dirname "$0")/build" && mpirun -np $NUM_RANKS ./sender_from_bp "$BP_FILE" "$CONTACT_NAME"

EXIT_CODE=$?

echo ""
echo "============================================================"
echo "Sender finished with exit code: $EXIT_CODE"

exit $EXIT_CODE
