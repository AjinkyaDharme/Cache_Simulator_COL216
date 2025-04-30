# Cache Simulator Project

## Overview
This project implements a parallel cache simulator that models the behavior of a multi-core processor cache hierarchy with MESI coherence protocol. The simulator analyzes cache performance by running trace files of memory access patterns and reports detailed statistics on execution cycles, miss rates, and bus traffic.

## Features
- Configurable cache parameters including:
  - Cache size (number of sets)
  - Associativity (2-way, 4-way, 8-way, etc.)
  - Block size (16B, 32B, 64B, etc.)
- MESI coherence protocol implementation
- LRU replacement policy
- Write-back, write-allocate policy
- Multi-core simulation (4 cores by default)
- Bus snooping mechanism
- Detailed performance statistics

## Requirements
- C++ compiler with C++11 support
- pthread library
- Make build system
- LaTeX (for documentation generation)

## Building the Project
To compile the simulator:
```bash
make
```
This will create an executable named `L1simulate` in the root directory.

## Running the Simulator
Basic usage:
```bash
./L1simulate -t <tracefile> -s <s> -E <E> -b <b> [-o <outfilename>]
```

Where:
- `-t <tracefile>`: Name of parallel application whose traces are used
- `-s <s>`: Number of set index bits (number of sets = 2^s)
- `-E <E>`: Associativity (number of cache lines per set)
- `-b <b>`: Number of block bits (block size = 2^b bytes)
- `-o <outfilename>`: Optional output file name (default: output.txt)
- `-h`: Display help message

Example:
```bash
./L1simulate -t resources/matrix.trace -s 5 -E 2 -b 5
```
This simulates a 4-core system with each core having a 4KB cache (32 sets, 2-way associativity, 32-byte blocks).

## Cache Performance Analysis
The simulator generates detailed statistics for each core, including:
- Total instructions processed
- Cache hit and miss rates
- Cache evictions and writebacks
- Bus invalidations
- Data traffic in bytes
- Execution cycles

To analyze how different cache parameters affect performance:
1. Run the simulator with different configurations
2. Compare execution cycles, miss rates, and other metrics
3. Find optimal configurations for specific workloads

## Documentation
To generate PDF documentation:
```bash
make doc
```
This will create a `report.pdf` file with detailed analysis of cache parameters and their impact on performance.

## Project Structure
- `src/`: Source code files
- `resources/`: Trace files for different applications
- `build/`: Compiled object files
- `bin/`: Binary executable
- `report.tex`: LaTeX source for documentation
- `cache_analysis.md`: Analysis of cache parameter impacts

## Key Findings
Our analysis shows that:
- Increasing the number of sets (cache size) provides the largest performance benefit
- 4-way associativity offers a good balance between performance and complexity
- 32-byte block size is optimal for most workloads, balancing spatial locality with transfer overhead

For typical workloads, the recommended configuration is 128 sets with 4-way associativity and 32-byte blocks, which provides the best performance/cost tradeoff.
