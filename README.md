# Resource Manager

A C++20 implementation of a lock-free resource manager with epoch-based reclamation.
Includes a benchmark program. This was created with the help of claude.ai.

## Features

- Thread-safe resource management
- Lock-free reader access
- Epoch-based memory reclamation
- Benchmark suite for performance testing

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 10+)
- CMake 3.16 or higher
- pthread support

## Building

```bash
# Create a build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build the project
cmake --build .
```

## Running Tests

```bash
# Or run the test executable directly
./resource_manager_test
```

## Running Benchmarks

```bash
# Run the benchmark with default settings
./resource_manager_benchmark

# Run with custom settings
./resource_manager_benchmark --readers 8 --duration 30 --updates 200

# For all options
./resource_manager_benchmark --help
```

## Build Options

- `-DCMAKE_BUILD_TYPE=Release` - Build with optimizations
- `-DCMAKE_BUILD_TYPE=Debug` - Build with debug information
- `-DCMAKE_INSTALL_PREFIX=/path/to/install` - Set installation path

## Installing

```bash
cmake --build . --target install
``` 
