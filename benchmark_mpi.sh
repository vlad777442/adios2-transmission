#!/bin/bash
# Benchmark script for testing different MPI rank combinations
# Usage: ./benchmark_mpi.sh [grid_size] [total_steps] [output_interval]

GRID_SIZE=${1:-128}
TOTAL_STEPS=${2:-1000}
OUTPUT_INTERVAL=${3:-100}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_FILE="${SCRIPT_DIR}/mpi_benchmark_results.csv"
HOSTFILE="${SCRIPT_DIR}/hostfile.txt"

# MPI configurations to test: "ranks nodes_description"
CONFIGS=(
    "1 single_node"
    "2 single_node"
    "4 single_node"
    "8 single_node"
    "4 multi_node"
    "8 multi_node"
    "12 multi_node"
    "16 multi_node"
    "20 multi_node"
    "24 multi_node"
    "32 multi_node"
)

echo "=== MPI Benchmark Suite ==="
echo "Grid size: ${GRID_SIZE}^3"
echo "Total steps: ${TOTAL_STEPS}"
echo "Output interval: ${OUTPUT_INTERVAL}"
echo "Results will be saved to: ${RESULTS_FILE}"
echo ""

# Create CSV header
echo "ranks,nodes,grid_size,total_steps,output_interval,total_time_s,outputs,avg_output_time_s,avg_throughput_MBps" > "${RESULTS_FILE}"

cd "${SCRIPT_DIR}/build"

# MPI options for multi-node
MPI_OPTS="--mca btl_tcp_if_include 10.10.1.0/24 --mca oob_tcp_if_include 10.10.1.0/24"

# Disable UCX to prevent SST segfaults with many ranks
export UCX_TLS=tcp
export UCX_NET_DEVICES=all

# Force use of /usr/local ADIOS2 (not system package with UCX bugs)
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

for CONFIG in "${CONFIGS[@]}"; do
    RANKS=$(echo $CONFIG | awk '{print $1}')
    NODE_TYPE=$(echo $CONFIG | awk '{print $2}')
    
    echo "============================================================"
    echo "Testing: ${RANKS} ranks (${NODE_TYPE})"
    echo "============================================================"
    
    # Create temp output file
    TEMP_OUTPUT=$(mktemp)
    
    if [ "$NODE_TYPE" == "multi_node" ]; then
        # Multi-node run with hostfile
        timeout 300 mpirun -np ${RANKS} ${MPI_OPTS} --hostfile "${HOSTFILE}" \
            -x UCX_TLS=tcp -x UCX_NET_DEVICES=all \
            -x LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH \
            ./gs_sender ${GRID_SIZE} ${TOTAL_STEPS} ${OUTPUT_INTERVAL} benchmark-test 2>&1 | tee "${TEMP_OUTPUT}"
    else
        # Single node run
        timeout 300 mpirun -np ${RANKS} \
            -x UCX_TLS=tcp -x UCX_NET_DEVICES=all \
            -x LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH \
            ./gs_sender ${GRID_SIZE} ${TOTAL_STEPS} ${OUTPUT_INTERVAL} benchmark-test 2>&1 | tee "${TEMP_OUTPUT}"
    fi
    
    # Parse results
    TOTAL_TIME=$(grep "Total time:" "${TEMP_OUTPUT}" | awk '{print $3}')
    NUM_OUTPUTS=$(grep "Total output steps:" "${TEMP_OUTPUT}" | awk '{print $4}')
    
    if [ -n "$TOTAL_TIME" ] && [ -n "$NUM_OUTPUTS" ]; then
        # Calculate average output time
        AVG_OUTPUT_TIME=$(echo "scale=4; $TOTAL_TIME / $NUM_OUTPUTS" | bc)
        
        # Calculate data size per output (2 * grid^3 * 8 bytes / 1024^2 = MB)
        DATA_PER_OUTPUT=$(echo "scale=2; 2 * $GRID_SIZE * $GRID_SIZE * $GRID_SIZE * 8 / 1048576" | bc)
        AVG_THROUGHPUT=$(echo "scale=2; $DATA_PER_OUTPUT / $AVG_OUTPUT_TIME" | bc)
        
        echo "${RANKS},${NODE_TYPE},${GRID_SIZE},${TOTAL_STEPS},${OUTPUT_INTERVAL},${TOTAL_TIME},${NUM_OUTPUTS},${AVG_OUTPUT_TIME},${AVG_THROUGHPUT}" >> "${RESULTS_FILE}"
        
        echo ""
        echo "Result: ${RANKS} ranks, ${TOTAL_TIME}s total, ${AVG_THROUGHPUT} MB/s avg throughput"
    else
        echo "${RANKS},${NODE_TYPE},${GRID_SIZE},${TOTAL_STEPS},${OUTPUT_INTERVAL},FAILED,0,0,0" >> "${RESULTS_FILE}"
        echo "Run failed or timed out"
    fi
    
    rm -f "${TEMP_OUTPUT}"
    
    # Brief pause between runs
    sleep 2
done

echo ""
echo "============================================================"
echo "=== Benchmark Complete ==="
echo "============================================================"
echo ""
echo "Results saved to: ${RESULTS_FILE}"
echo ""
cat "${RESULTS_FILE}" | column -t -s','
