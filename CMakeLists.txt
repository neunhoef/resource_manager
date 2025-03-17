cmake_minimum_required(VERSION 3.16)
project(ResourceManager VERSION 0.1.0 LANGUAGES CXX)

# Set C++20 as the standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include custom cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Define targets
add_executable(resource_manager_test src/ResourceManagerTest.cpp)
add_executable(resource_manager_benchmark src/benchmark.cpp)
add_executable(bounded_list_benchmark src/bench_bounded.cpp)

# Set compiler options for all targets
set_compiler_options(resource_manager_test)
set_compiler_options(resource_manager_benchmark)
set_compiler_options(bounded_list_benchmark)

# Install targets
install(TARGETS resource_manager_test resource_manager_benchmark bounded_list_benchmark
        RUNTIME DESTINATION bin)

# Enable testing
enable_testing()
add_test(NAME ResourceManagerTest COMMAND resource_manager_test)

# Add a custom target for running benchmarks
add_custom_target(benchmark
    COMMAND resource_manager_benchmark
    DEPENDS resource_manager_benchmark
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running resource manager benchmarks"
)

# Add a custom target for running bounded list benchmarks
add_custom_target(bench_bounded
    COMMAND bounded_list_benchmark
    DEPENDS bounded_list_benchmark
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running bounded list benchmarks"
) 
