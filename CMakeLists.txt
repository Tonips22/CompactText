cmake_minimum_required(VERSION 3.18)
project(CompactText CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(compacttext_seq src/sequential.cpp)

# OpenMP
find_package(OpenMP REQUIRED)
add_executable(compacttext_omp src/omp.cpp)
target_link_libraries(compacttext_omp PUBLIC OpenMP::OpenMP_CXX)

# MPI
find_package(MPI REQUIRED)
add_executable(compacttext_mpi src/mpi.cpp)
target_link_libraries(compacttext_mpi PUBLIC MPI::MPI_CXX)