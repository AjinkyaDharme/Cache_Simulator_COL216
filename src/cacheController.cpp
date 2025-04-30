#include <iostream>
#include <fstream>
#include <thread>
#include <regex>
#include <cmath>
#include <functional>

// USER-DEFINED LIBRARIES
#include "cacheController.h"
#include "cacheStructure.h"
#include "cacheAccess.h"
#include "busOperation.h"

//Initialize the particular cache and other parameters
CacheController::CacheController(ConfigInfo config, char* traceFile, int threadId)
{
    
    this -> config = config;
    this -> inputFile = std::string(traceFile);
    this -> outputFile = this -> inputFile + ".out";
    
    this -> numByteOffsetBits = log2(config.blockSize);
    this -> numIndexBits = log2(config.numberIndicies);

    // Initialize the counters for this particular cache
    this -> globalCycles = 0;
    this -> globalHits = 0;
    this -> globalMisses = 0;
    this -> globalEvictions = 0;
	this -> threadId = threadId;
    this -> idleCycles = 0;
    this -> executionCompleted = false;

	// form a function pointer to onBusresponse()
	funcPointer fp = std::bind(&CacheController::onBusresponse, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	this -> fp = fp;

    // Create Cache
    std::vector <std::vector <cacheEntry> > cache;  
    for(size_t i = 0; i < config.numberIndicies; i++)
    {
        std::vector <cacheEntry> cacheSet(config.associativity);
        cache.push_back(cacheSet);
    }
    this -> cache = cache;

	// Initialize statistics

    totalInstructions = 0;
    totalReads = 0;
    writebacks = 0;
    busInvalidations = 0;
    dataTrafficBytes = 0;
    
}

void CacheController::incrementIdleCycles(unsigned int cycles) {
    // Only increment idle cycles if this core hasn't completed execution
    if (!executionCompleted) {
        idleCycles += cycles;
    }
}

void CacheController::onBusresponse(unsigned int index, unsigned long int tag, std::string message)
{
    auto& cacheSet = cache.at(index);
    if(message == "read") {
        for(auto itr = cacheSet.begin(); itr != cacheSet.end(); itr++) {
            if(itr->second == tag && itr->first.first.first == 1) {
                itr->first.second = 1; // Set shared bit
                break;
            }
        }
        return;    
    }

    if(message == "write") {
        for(auto itr = cacheSet.begin(); itr != cacheSet.end(); itr++) {
            if(itr->second == tag && itr->first.first.first == 1) {
                if(itr->first.first.second == 1) { // If block is dirty
                    // dataTrafficBytes += config.blockSize; // Track writeback traffic
                    // writebacks++;
                }
                
                // busInvalidations++;
                itr->first.first.first = 0;
                cacheEntry block = *itr;
                cacheSet.erase(itr); 
                cacheSet.emplace(cacheSet.end(), block);
                break;
            }
        }
    }
    if (message == "notSharedAnymore")
	{
		for (auto itr = cacheSet.begin(); itr != cacheSet.end(); itr++)
		{
			if (itr->second == tag && itr->first.first.first == 1)
			{
				itr->first.second = 0;   //Changed to modified state
				break;
			}
		}
		return;
	}
}

CacheController::LineState CacheController::getLineState(unsigned int idx, unsigned long tag) const {
    auto& set = cache.at(idx);
    for (auto const & entry : set) {
        if (entry.second == tag && entry.first.first.first /* valid */) {
            // Dirty bit == Modified
            if (entry.first.first.second) return LineState::Modified;
            // Shared bit == Exclusive vs Shared
            return entry.first.second ? LineState::Shared : LineState::Exclusive;
        }
    }
    return LineState::Invalid;
}


CacheController::AddressInfo CacheController::getAddressInfo(unsigned long int address)
{	
	AddressInfo ai;

	ai.tag = address >> (numByteOffsetBits + numIndexBits);
	unsigned long int offset = static_cast<unsigned long int>(std::pow(2, static_cast<double>(numByteOffsetBits)));
	ai.setIndex = (address / offset) % (static_cast<unsigned long int>(std::pow(2, static_cast<double>(numIndexBits))));

	return ai;
}

void CacheController::cacheAccess(CacheResponse* response, bool isWrite, unsigned long int address, 
                                            std::mutex& mutex, std::condition_variable& convar, bus& Bus)
{
    response->hit = 0;
    response->eviction = 0;
    response->dirtyEviction = 0;
    response->cycles = 0;
    response->trafficBytes = 0;
    AddressInfo ai = getAddressInfo(address);
    funcPointer fp = std::bind(&CacheController::onBusresponse, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    // Check if the instruction is a read or write instruction
    if (isWrite)
        writebacks += writeOnCache(ai.setIndex, ai.tag, cache.at(ai.setIndex), response, config, mutex, convar, Bus, threadId, fp);
    else {
        writebacks += readFromCache(ai.setIndex, ai.tag, cache.at(ai.setIndex), response, config, mutex, convar, Bus, threadId, fp);
        totalReads++;
    }
    globalCycles += response->cycles;

    if (response->hit == 0)
        globalMisses++;
    else
        globalHits++;

    if (response->eviction != 0)
        globalEvictions++;

    totalInstructions++;
    dataTrafficBytes += response->trafficBytes;
}

void CacheController::runTracefile(std::mutex& mutex, std::condition_variable& convar, bus& Bus) 
{
    // process each input line
    std::string line;
   
    std::ofstream outfile(outputFile);
    std::ifstream infile(inputFile);
    
    while (getline(infile, line)) {
        // Skip comments or empty lines
        if (line.empty() || line.substr(0, 2) == "==") {
            continue;
        }
        
        unsigned long int address;
        CacheResponse response;
        
        // Parse based on first character
        if (line.length() >= 3 && line[0] == 'R' && line[1] == ' ') {
            // Read operation
            std::istringstream hexStream(line.substr(2)); // Skip "R "
            hexStream >> std::hex >> address;
            outfile << line.substr(0, line.find(' ') + 1) << line.substr(2);
            cacheAccess(&response, false, address, mutex, convar, Bus);
            outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
        } 
        else if (line.length() >= 3 && line[0] == 'W' && line[1] == ' ') {
            // Write operation
            std::istringstream hexStream(line.substr(2)); // Skip "W "
            hexStream >> std::hex >> address;
            outfile << line.substr(0, line.find(' ') + 1) << line.substr(2);
            cacheAccess(&response, true, address, mutex, convar, Bus);
            outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
        } 
        else {
			
			throw std::runtime_error("Encountered unknown line format in tracefile.");

        }
        outfile << std::endl;
    }

    executionCompleted = true; // Mark this core as completed
}
	
	std::string CacheController::getStatistics() {
	
		double cacheMissRate = (totalInstructions > 0) ? 
	
						(static_cast<double>(globalMisses) / (totalInstructions)) * 100 : 0.0;
	
	
	
		std::ostringstream stats;
	
		stats << "Core " << threadId << " Statistics:\n";
	
		stats << "Total Instructions: " << totalInstructions << "\n";
	
		stats << "Total Reads: " << totalReads << "\n";
	
		stats << "Total Writes: " << totalInstructions - totalReads << "\n";
	
		stats << "Total Execution Cycles: " << globalCycles << "\n";
	
		stats << "Idle Cycles: " << idleCycles << "\n";
	
		stats << "Cache Misses: " << globalMisses << "\n";
	
		stats << "Cache Miss Rate: " << cacheMissRate << "%\n";
	
		stats << "Cache Evictions: " << globalEvictions << "\n";
	
		stats << "Writebacks: " << writebacks << "\n";
		stats << "Bus Invalidations: " << busInvalidations << "\n";
	
		stats << "Data Traffic (Bytes): " << dataTrafficBytes << "\n\n";
	
		return stats.str();
	
	}

unsigned long long CacheController::getDataTrafficBytes() const {
    return dataTrafficBytes;
}

