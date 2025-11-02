#!/bin/bash
#
# Local test script for ADIOS2 transmission
# Runs sender and receiver on the same node for testing
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

cd "$SCRIPT_DIR"

# Clean up any old contact files
rm -f data-transfer.sst* 2>/dev/null

echo "========================================="
echo "ADIOS2 Local Transmission Test"
echo "========================================="
echo ""
echo "This will test sender and receiver on the same node."
echo ""

# Start sender in background
echo "[1/3] Starting sender..."
mpirun -np 1 "$BUILD_DIR/sender" data-transfer > sender.log 2>&1 &
SENDER_PID=$!
echo "Sender started (PID: $SENDER_PID)"

# Wait for sender to create contact file
echo "[2/3] Waiting for contact file..."
TIMEOUT=10
COUNT=0
while [ ! -f "data-transfer.sst" ] && [ $COUNT -lt $TIMEOUT ]; do
    sleep 1
    COUNT=$((COUNT + 1))
    echo -n "."
done
echo ""

if [ ! -f "data-transfer.sst" ]; then
    echo "ERROR: Sender did not create contact file after ${TIMEOUT}s"
    echo "Check sender.log for details"
    kill $SENDER_PID 2>/dev/null || true
    cat sender.log
    exit 1
fi

echo "Contact file created successfully!"

# Start receiver
echo "[3/3] Starting receiver..."
mpirun -np 1 "$BUILD_DIR/receiver" data-transfer > receiver.log 2>&1 &
RECEIVER_PID=$!
echo "Receiver started (PID: $RECEIVER_PID)"

echo ""
echo "========================================="
echo "Both processes running:"
echo "  Sender:   PID $SENDER_PID"
echo "  Receiver: PID $RECEIVER_PID"
echo "========================================="
echo ""
echo "Waiting for completion..."

# Wait for both processes
wait $SENDER_PID 2>/dev/null
SENDER_EXIT=$?
wait $RECEIVER_PID 2>/dev/null
RECEIVER_EXIT=$?

echo ""
echo "========================================="
echo "Test Complete"
echo "========================================="
echo "Sender exit code:   $SENDER_EXIT"
echo "Receiver exit code: $RECEIVER_EXIT"
echo ""

if [ $SENDER_EXIT -eq 0 ] && [ $RECEIVER_EXIT -eq 0 ]; then
    echo "✓ SUCCESS: Both sender and receiver completed successfully!"
else
    echo "✗ FAILURE: One or more processes failed"
fi

echo ""
echo "View logs:"
echo "  cat sender.log"
echo "  cat receiver.log"
echo ""

if [ -f "transfer_metrics.csv" ]; then
    echo "Transfer metrics: transfer_metrics.csv"
fi

# Clean up
rm -f data-transfer.sst* 2>/dev/null

exit $((SENDER_EXIT + RECEIVER_EXIT))
