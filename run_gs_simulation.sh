#!/bin/bash
# Run Gray-Scott simulation with SST streaming (multi-node support)

# Default values
GRID_SIZE=${1:-128}
TOTAL_STEPS=${2:-10000}
OUTPUT_INTERVAL=${3:-100}
CONTACT_NAME=${4:-gs-simulation}
NUM_RANKS=${5:-4}
HOSTFILE=${6:-}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Gray-Scott Simulation Launcher ==="
echo ""
echo "Grid size: ${GRID_SIZE}^3"
echo "Total simulation steps: ${TOTAL_STEPS}"
echo "Output every: ${OUTPUT_INTERVAL} steps"
echo "Contact name: ${CONTACT_NAME}"
echo "MPI ranks: ${NUM_RANKS}"
if [ -n "$HOSTFILE" ]; then
    echo "Hostfile: ${HOSTFILE}"
    echo "Nodes: $(cat ${SCRIPT_DIR}/${HOSTFILE} | grep -v '^#' | awk '{print $1}' | tr '\n' ' ')"
fi
echo ""
echo "Make sure the receiver is running first!"
echo ""
echo "Starting simulation..."
echo "============================================================"
echo ""

cd "${SCRIPT_DIR}/build"

# MPI options for multi-node runs
MPI_OPTS="--mca btl_tcp_if_include 10.10.1.0/24 --mca oob_tcp_if_include 10.10.1.0/24"

if [ -n "$HOSTFILE" ]; then
    mpirun -np ${NUM_RANKS} ${MPI_OPTS} --hostfile "${SCRIPT_DIR}/${HOSTFILE}" ./gs_sender ${GRID_SIZE} ${TOTAL_STEPS} ${OUTPUT_INTERVAL} ${CONTACT_NAME}
else
    mpirun -np ${NUM_RANKS} ./gs_sender ${GRID_SIZE} ${TOTAL_STEPS} ${OUTPUT_INTERVAL} ${CONTACT_NAME}
fi

EXIT_CODE=$?

echo ""
echo "============================================================"
echo "Simulation finished with exit code: ${EXIT_CODE}"
