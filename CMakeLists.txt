cmake_minimum_required(VERSION 3.12)
project(ADIOS2_WAN_Transfer C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(MPI REQUIRED)
find_package(ADIOS2 REQUIRED)

# Include directories
include_directories(${MPI_CXX_INCLUDE_DIRS})

# Sender executable
add_executable(sender sender.cpp)
target_link_libraries(sender 
    adios2::adios2
    MPI::MPI_CXX
)

# Receiver executable
add_executable(receiver receiver.cpp)
target_link_libraries(receiver 
    adios2::adios2
    MPI::MPI_CXX
)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Print configuration info
message(STATUS "==========================================")
message(STATUS "ADIOS2 WAN Transfer Configuration")
message(STATUS "==========================================")
message(STATUS "MPI Found: ${MPI_FOUND}")
message(STATUS "MPI C++ Compiler: ${MPI_CXX_COMPILER}")
message(STATUS "ADIOS2 Found: ${ADIOS2_FOUND}")
message(STATUS "ADIOS2 Version: ${ADIOS2_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "==========================================")
