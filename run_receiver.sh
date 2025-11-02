#!/bin/bash
# run_receiver.sh - Start receiver on Clemson
# Usage: ./run_receiver.sh <contact-file-from-sender>

if [ $# -eq 0 ]; then
    echo "Usage: $0 <contact-file>"
    echo ""
    echo "Example:"
    echo "  # First, copy contact file from sender:"
    echo "  scp sender-host:/path/to/data-transfer.sst ./"
    echo ""
    echo "  # Then run receiver:"
    echo "  $0 data-transfer.sst"
    exit 1
fi

CONTACT_FILE="$1"

if [ ! -f "$CONTACT_FILE" ]; then
    echo "ERROR: Contact file not found: $CONTACT_FILE"
    echo ""
    echo "Please copy the contact file from the sender machine first:"
    echo "  scp sender-host:/path/to/contact-file ./"
    exit 1
fi

echo "=== ADIOS2 Receiver (Clemson) ==="
echo "Using contact file: $CONTACT_FILE"
echo ""

# Run receiver - pass the contact file name without .sst extension
CONTACT_NAME="${CONTACT_FILE%.sst}"
./build/receiver "$CONTACT_NAME"

EXIT_CODE=$?

echo ""
echo "Receiver finished with exit code: $EXIT_CODE"

exit $EXIT_CODE
