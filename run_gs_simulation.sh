#!/bin/bash
# Run Gray-Scott simulation with SST streaming

# Default values
GRID_SIZE=${1:-128}
TOTAL_STEPS=${2:-10000}
OUTPUT_INTERVAL=${3:-100}
CONTACT_NAME=${4:-gs-simulation}
NUM_RANKS=${5:-4}

echo "=== Gray-Scott Simulation Launcher ==="
echo ""
echo "Grid size: ${GRID_SIZE}^3"
echo "Total simulation steps: ${TOTAL_STEPS}"
echo "Output every: ${OUTPUT_INTERVAL} steps"
echo "Contact name: ${CONTACT_NAME}"
echo "MPI ranks: ${NUM_RANKS}"
echo ""
echo "Make sure the receiver is running first!"
echo ""
echo "Starting simulation..."
echo "============================================================"
echo ""

cd "$(dirname "$0")/build"

mpirun -np ${NUM_RANKS} ./gs_sender ${GRID_SIZE} ${TOTAL_STEPS} ${OUTPUT_INTERVAL} ${CONTACT_NAME}

EXIT_CODE=$?

echo ""
echo "============================================================"
echo "Simulation finished with exit code: ${EXIT_CODE}"
