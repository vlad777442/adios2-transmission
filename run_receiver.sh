#!/bin/bash
# run_receiver.sh - Start receiver (Run this FIRST on Clemson/Node1)

echo "=== ADIOS2 Receiver (Step 1: Run this FIRST) ==="
echo ""
echo "This will start the receiver and wait for the sender to connect."
echo ""
echo "After this starts, go to the SENDER machine and run:"
echo "  ./run_sender.sh"
echo ""
echo "Starting receiver..."
echo "============================================================"
echo ""

# Run receiver
cd build && ./receiver

EXIT_CODE=$?

echo ""
echo "============================================================"
echo "Receiver finished with exit code: $EXIT_CODE"

exit $EXIT_CODE
