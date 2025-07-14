cmake_minimum_required(VERSION 3.18)

# El proyecto ahora tiene C y C++
project(CompactText LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# -------- SECUENCIAL (C) --------
add_executable(compacttext_seq src/sequential.c)

# -------- OpenMP (C++) ---------
find_package(OpenMP REQUIRED)
add_executable(compacttext_omp src/omp.cpp)
target_link_libraries(compacttext_omp PUBLIC OpenMP::OpenMP_CXX)

# -------- MPI (C++) ------------
find_package(MPI REQUIRED)
add_executable(compacttext_mpi src/mpi.cpp)
target_link_libraries(compacttext_mpi PUBLIC MPI::MPI_CXX)
