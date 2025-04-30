#include <iostream>
#include <fstream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <cmath>
#include <string>
#include <cstdlib>

#include "cacheconf.h"
#include "cacheController.h"
#include "busOperation.h"

// Global pointer array to CacheController objects
CacheController** controllers = nullptr;
int numThreads = 4; // Number of threads = number of trace files
extern unsigned long int totalBusTransactions;
void printHelp() {
    std::cout << "Usage: ./L1simulate -h -t <tracefile> -s <s> -E <E> -b <b> -o <outfilename>\n";
    std::cout << "Options:\n";
    std::cout << "  -h                Prints this help message.\n";
    std::cout << "  -t <tracefile>    Name of parallel application (e.g., app1) whose 4 traces are to be used in simulation.\n";
    std::cout << "  -s <s>            Number of set index bits (number of sets in the cache = S = 2^s).\n";
    std::cout << "  -E <E>            Associativity (number of cache lines per set).\n";
    std::cout << "  -b <b>            Number of block bits (block size = B = 2^b).\n";
    std::cout << "  -o <outfilename>  Logs output in the specified file for plotting, etc.\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string tracefile;
    int s = -1, E = -1, b = -1;
    std::string outfilename = "output.txt";

    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-h") {
            printHelp();
            return 0;
        } else if (std::string(argv[i]) == "-t" && i + 1 < argc) {
            tracefile = argv[++i];
        } else if (std::string(argv[i]) == "-s" && i + 1 < argc) {
            s = std::atoi(argv[++i]);
        } else if (std::string(argv[i]) == "-E" && i + 1 < argc) {
            E = std::atoi(argv[++i]);
        } else if (std::string(argv[i]) == "-b" && i + 1 < argc) {
            b = std::atoi(argv[++i]);
        } else if (std::string(argv[i]) == "-o" && i + 1 < argc) {
            outfilename = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << argv[i] << "\n";
            printHelp();
            return 1;
        }
    }

    if (tracefile.empty() || s == -1 || E == -1 || b == -1) {
        std::cerr << "Missing required arguments.\n";
        printHelp();
        return 1;
    }

    // Write parameters to testconfig file
    std::ofstream configFile("src/testconfig");
    configFile << (1 << s) << "\n"; // Number of sets
    configFile << (1 << b) << "\n"; // Block size
    configFile << E << "\n";        // Associativity
    configFile << "1\n";            // Replacement policy (LRU)
    configFile << "1\n";            // Write policy (Write-back, Write-allocate)
    configFile << "1\n";            // Cache access cycles
    configFile << "100\n";          // Memory access cycles
    configFile << "1\n";            // MESI protocol enabled
    configFile.close();

    // Update output filename
    std::ofstream outputFile(outfilename);
    outputFile.close();

    ConfigInfo config;
    unsigned int tmp;
    int numThreads = 4; // Number of threads = number of trace files
    std::ifstream configFileInput("src/testconfig");
    configFileInput >> config.numberIndicies;
    configFileInput >> config.blockSize;
    configFileInput >> config.associativity;
    configFileInput >> tmp;
    config.replacementPolicy = static_cast<ReplacementPolicy>(tmp);
    configFileInput >> tmp;
    config.writePolicy = static_cast<WritePolicy>(tmp);
    configFileInput >> config.cacheAccessCycles;
    configFileInput >> config.memoryAccessCycles;
    configFileInput >> tmp;
    config.protocol = static_cast<CoherenceProtocol>(tmp);
    configFileInput.close();

    std::thread threads[numThreads];
    std::mutex mutex;
    std::condition_variable convar;
    bus Bus;

    // Allocate the global pointer array for CacheController objects
    controllers = new CacheController*[numThreads];

    for (int i = 0; i < numThreads; i++) {
        std::string traceFileName = "resources/" + tracefile + "_proc" + std::to_string(i) + ".trace"; // Specify resources folder
        controllers[i] = new CacheController(config, const_cast<char*>(traceFileName.c_str()), i); // Use const_cast to match char* type
        threads[i] = std::thread(&CacheController::runTracefile, controllers[i], std::ref(mutex), std::ref(convar), std::ref(Bus));
    }

    for (int i = 0; i < numThreads; i++) {
        threads[i].join();
    }

    

    // Write simulation parameters and statistics to output file
    std::ofstream finalOutput(outfilename);
    finalOutput << "Simulation Parameters:\n";
    finalOutput << "Trace Prefix: " << tracefile << "\n";
    finalOutput << "Set Index Bits: " << static_cast<int>(std::log2(config.numberIndicies)) << "\n";
    finalOutput << "Associativity: " << config.associativity << "\n";
    finalOutput << "Block Bits: " << static_cast<int>(std::log2(config.blockSize)) << "\n";
    finalOutput << "Block Size (Bytes): " << config.blockSize << "\n";
    finalOutput << "Number of Sets: " << config.numberIndicies << "\n";
    finalOutput << "Cache Size (KB per core): " 
                << (config.numberIndicies * config.blockSize * config.associativity) / 1024 << "\n";
    finalOutput << "MESI Protocol: " << "Enabled" << "\n";
    finalOutput << "Write Policy: " 
                << "Write-back, Write-allocate" << "\n";
    finalOutput << "Replacement Policy: " 
                << "LRU" << "\n";
    finalOutput << "Bus: Central snooping bus\n\n";

    unsigned long long totalTrafficBytes = 0; // Variable to store the sum of traffic bytes
    unsigned int maxExecutionCycles = 0;
    for (int i = 0; i < numThreads; i++) {
        finalOutput << controllers[i]->getStatistics();
        totalTrafficBytes += controllers[i]->getDataTrafficBytes(); 
        
        
        unsigned int coreCycles = controllers[i]->getTotalExecutionCycles();
        if (coreCycles > maxExecutionCycles) {
            maxExecutionCycles = coreCycles;
        }
    }

    finalOutput << "Overall Bus Summary:"<< "\n";
    finalOutput << "Total Bus Transactions: " << totalBusTransactions<<"\n";
    finalOutput << "Total Bus Traffic (Bytes): " << totalTrafficBytes << "\n";

    finalOutput.close();

    // Clean up the global pointer array
    delete[] controllers;

    std::cout << "Simulated\n";
    return 0;
}