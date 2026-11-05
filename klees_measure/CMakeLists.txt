cmake_minimum_required(VERSION 3.12)
project(KMP_SDC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /W4)
else()
    add_compile_options(-O3 -march=native -Wall -Wextra)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/harness
    ${CMAKE_CURRENT_SOURCE_DIR}/solver
)

# Header-only core library
add_library(kmp_core INTERFACE)
target_include_directories(kmp_core INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/harness
    ${CMAKE_CURRENT_SOURCE_DIR}/solver
)

# Main CLI solver and research evaluation harness
add_executable(kmp_bench solver/main.cpp)
target_link_libraries(kmp_bench PRIVATE kmp_core)

# Scaling grid benchmark executable
add_executable(kmp_scaling benchmarks/scaling_grid.cpp)
target_link_libraries(kmp_scaling PRIVATE kmp_core)

# Tensor clean cell verification executable
add_executable(test_tensor_clean_cell tests/test_tensor_clean_cell.cpp)
target_link_libraries(test_tensor_clean_cell PRIVATE kmp_core)

# CTest integration
enable_testing()
add_test(NAME VerificationTest COMMAND kmp_bench --mode test)
add_test(NAME TensorCleanCellTest COMMAND test_tensor_clean_cell)
