#include <iostream>
#include <fstream>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "cacheconf.h"
#include "cacheController.h"
#include "busOperation.h"

int main(int argc, char *argv[])
{
    ConfigInfo config;
    if (argc < 3) {
        std::cerr << "You need at least two command line arguments. Provide a configuration file and at least one trace file." << std::endl;
        return 1;
    }

    unsigned int tmp;
    int numThreads = argc - 2; // Number of threads = number of trace files
    std::ifstream configFile(argv[1]);
    configFile >> config.numberIndicies;
    configFile >> config.blockSize;
    configFile >> config.associativity;
    configFile >> tmp;
    config.replacementPolicy = static_cast<ReplacementPolicy>(tmp);
    configFile >> tmp;
    config.writePolicy = static_cast<WritePolicy>(tmp);
    configFile >> config.cacheAccessCycles;
    configFile >> config.memoryAccessCycles;
    configFile >> tmp;
    config.protocol = static_cast<CoherenceProtocol>(tmp);
    configFile.close();

    std::thread threads[numThreads];
    std::mutex mutex;
    std::condition_variable convar;
    bus Bus;

    // Create an array of CacheController objects
    CacheController* controllers[numThreads];

    for (int i = 0; i < numThreads; i++) {
        controllers[i] = new CacheController(config, argv[2 + i], i);
        threads[i] = std::thread(&CacheController::runTracefile, controllers[i], std::ref(mutex), std::ref(convar), std::ref(Bus));
        std::cout << "Thread " << i << ": Starting, Waiting\n";
    }

    for (int i = 0; i < numThreads; i++) {
        threads[i].join();
    }

    // Write statistics to simulation_output.txt
    std::ofstream finalOutput("simulation_output.txt");
    for (int i = 0; i < numThreads; i++) {
        finalOutput << controllers[i]->getStatistics();
        delete controllers[i]; // Clean up dynamically allocated memory
    }
    finalOutput.close();

    std::cout << "Program exit\n";
    return 0;
}