#!/bin/bash
# Setup script to deploy and build on remote nodes

NODES="${1:-node1 node2}"
SRC_DIR="/users/vlad777/research/adios2-transmission"

echo "=== Multi-Node Setup Script ==="
echo "Source: $SRC_DIR"
echo "Target nodes: $NODES"
echo ""

for NODE in $NODES; do
    echo "----------------------------------------"
    echo "Setting up $NODE..."
    echo "----------------------------------------"
    
    # Create directory structure
    ssh vlad777@$NODE "mkdir -p $SRC_DIR/build"
    
    # Copy source files
    echo "Copying source files..."
    scp -q $SRC_DIR/*.cpp $SRC_DIR/*.sh $SRC_DIR/CMakeLists.txt vlad777@$NODE:$SRC_DIR/
    
    # Build on remote node
    echo "Building on $NODE..."
    ssh vlad777@$NODE "cd $SRC_DIR/build && cmake .. && make -j4 gs_sender" 2>&1 | tail -5
    
    # Verify
    if ssh vlad777@$NODE "test -x $SRC_DIR/build/gs_sender"; then
        echo "✓ $NODE: gs_sender built successfully"
    else
        echo "✗ $NODE: Build failed!"
    fi
    echo ""
done

echo "=== Setup Complete ==="
echo ""
echo "Test MPI across nodes:"
echo "  mpirun -np 4 --hostfile hostfile.txt hostname"
