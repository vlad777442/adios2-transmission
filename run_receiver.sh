#!/bin/bash
# run_receiver.sh - Start receiver on Clemson
# Usage: ./run_receiver.sh [contact-file-path]

CONTACT_FILE="${1:-data-transfer.sst}"

echo "=== ADIOS2 Receiver (Clemson) ==="
echo "Contact file will be: $PWD/$CONTACT_FILE"
echo ""

# Clean up old contact files
rm -f "$CONTACT_FILE" "${CONTACT_FILE}."* 2>/dev/null

# Set contact file location
export ADIOS2_SST_CONTACT_INFO_FILE="$PWD/$CONTACT_FILE"

# Display instructions
echo "=========================================="
echo "Receiver will create contact file:"
echo "  $PWD/$CONTACT_FILE"
echo ""
echo "On sender machine (Utah), run:"
echo "  scp $(hostname):$PWD/$CONTACT_FILE /path/to/sender/directory/"
echo "=========================================="
echo ""

# Run receiver
./receiver

EXIT_CODE=$?

echo ""
echo "Receiver finished with exit code: $EXIT_CODE"

# Clean up
rm -f "$CONTACT_FILE" "${CONTACT_FILE}."* 2>/dev/null

exit $EXIT_CODE
