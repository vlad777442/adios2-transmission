#!/bin/bash
# run_sender.sh - Start sender (Run this SECOND on Utah/Node0)

echo "=== ADIOS2 Sender (Step 2: Run this SECOND) ==="
echo ""
echo "Make sure the receiver is already running and waiting!"
echo ""
echo "Starting sender..."
echo "============================================================"
echo ""

# Run sender
cd build && ./sender

EXIT_CODE=$?

echo ""
echo "============================================================"
echo "Sender finished with exit code: $EXIT_CODE"

exit $EXIT_CODE
