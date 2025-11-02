#!/bin/bash
# run_sender.sh - Start sender on Utah
# Usage: ./run_sender.sh <contact-file-from-receiver>

if [ $# -eq 0 ]; then
    echo "Usage: $0 <contact-file>"
    echo ""
    echo "Example:"
    echo "  # First, copy contact file from receiver:"
    echo "  scp clemson-host:/path/to/data-transfer.sst ./"
    echo ""
    echo "  # Then run sender:"
    echo "  $0 data-transfer.sst"
    exit 1
fi

CONTACT_FILE="$1"

if [ ! -f "$CONTACT_FILE" ]; then
    echo "ERROR: Contact file not found: $CONTACT_FILE"
    echo ""
    echo "Please copy the contact file from the receiver machine first:"
    echo "  scp receiver-host:/path/to/contact-file ./"
    exit 1
fi

echo "=== ADIOS2 Sender (Utah) ==="
echo "Using contact file: $CONTACT_FILE"
echo ""

# Set contact file location
export ADIOS2_SST_CONTACT_INFO_FILE="$PWD/$CONTACT_FILE"

# Run sender
./sender

EXIT_CODE=$?

echo ""
echo "Sender finished with exit code: $EXIT_CODE"

exit $EXIT_CODE
