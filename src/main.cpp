#include <iostream>
#include <fstream>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "cacheconf.h"
#include "cacheController.h"
#include "busOperation.h"

void initializeCache(ConfigInfo config, char* traceFile, int threadId, std::mutex &mutex, std::condition_variable &convar, bus &Bus)
{   
    //Initialize cache for all the 4 cores
    CacheController cache = CacheController(config, traceFile, threadId);
    cache.runTracefile(mutex, convar, Bus);
}

int main(int argc, char *argv[])
{
    ConfigInfo config;
     if (argc < 3)
     {
        std::cerr << "you need at least two command line arguments. you should provide a configuration file and at least one trace file." << std::endl;
	 	return 1;
     }
    unsigned int tmp;
    int numThreads = argc - 2; // Define the number of threads, if we pass one trace file => 1 thread, 2 tracefiles => 2 threads
	// Read the configuration file
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
    
    for(int i = 0; i < numThreads; i++)
	{
		threads[i] = std::thread(initializeCache, config, argv[2 + i], i, std::ref(mutex), std::ref(convar), std::ref(Bus));	// For each iteration, make a new thread and call function thrSync()
		std::cout << "Thread " << i << ": Starting, Waiting\n";		
	}

    
    for(int i = 0; i < numThreads; i++)
	{
		threads[i].join();
	}
    std::cout << "Program exit\n";
    return 0;
}