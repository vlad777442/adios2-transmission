#!/bin/bash
#
# Example SLURM job script for running the receiver on Clemson HPC
# Adjust parameters based on your cluster configuration
#

#SBATCH --job-name=adios2-receiver
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=4
#SBATCH --time=01:00:00
#SBATCH --partition=standard
#SBATCH --account=your-account

# Load required modules
module load gcc
module load openmpi
module load adios2

# Set environment variables
export ADIOS2_SST_CONTACT_INFO_FILE=/shared/path/data-transfer.sst

# Change to working directory
cd $SLURM_SUBMIT_DIR

# Run the receiver
echo "Starting ADIOS2 receiver at Clemson"
echo "Job ID: $SLURM_JOB_ID"
echo "Nodes: $SLURM_JOB_NUM_NODES"
echo "Tasks: $SLURM_NTASKS"
echo "========================================="

mpirun -np $SLURM_NTASKS ./build/bin/receiver

echo "========================================="
echo "Reception complete"
echo "Metrics saved to: $PWD/transfer_metrics.csv"
