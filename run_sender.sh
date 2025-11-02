#!/bin/bash
# run_sender.sh - Start sender on Utah
# Usage: ./run_sender.sh [contact-file-name]

CONTACT_FILE="${1:-data-transfer}"

echo "=== ADIOS2 Sender (Utah) ==="
echo "Will create contact file: $PWD/$CONTACT_FILE.sst"
echo ""

# Clean up old contact files
rm -f "${CONTACT_FILE}.sst" "${CONTACT_FILE}.sst."* 2>/dev/null

# Display instructions
echo "=========================================="
echo "Sender will create contact file:"
echo "  $PWD/$CONTACT_FILE.sst"
echo ""
echo "On receiver machine (Clemson), copy this file:"
echo "  scp $(hostname):$PWD/$CONTACT_FILE.sst /path/to/receiver/directory/"
echo ""
echo "Then start receiver with:"
echo "  ./run_receiver.sh $CONTACT_FILE.sst"
echo "=========================================="
echo ""

# Run sender
./build/sender "$CONTACT_FILE"

EXIT_CODE=$?

echo ""
echo "Sender finished with exit code: $EXIT_CODE"
echo ""
echo "Contact file created: $PWD/$CONTACT_FILE.sst"
echo "Copy this to the receiver machine to establish connection."

# Clean up contact files
rm -f "${CONTACT_FILE}.sst" "${CONTACT_FILE}.sst."* 2>/dev/null

exit $EXIT_CODE
