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

void CacheController::onBusresponse(unsigned int index, unsigned long int tag, std::string message)
{
	
	auto cacheSet = cache.at(index);
	if(message == "read")
	{
		for(auto itr = cacheSet.begin(); itr != cacheSet.end(); itr++)
		{
        	if(itr -> second == tag && itr -> first.first.first == 1)
			{
				itr -> first.second = 1; // set shared bit to one
				break;
			}

		}
		return;	
	}

	if(message == "write")
	{
		for(auto itr = cacheSet.begin(); itr != cacheSet.end(); itr++)
		{
			if(itr -> second == tag && itr -> first.first.first == 1)
			{
				if (itr->first.first.second == 1) // Assuming second bit indicates dirty state
					{
						writebacks++; // Increment writeback counter
					}
				itr -> first.first.first = 0; // Put Valid bit to 0
				cacheEntry block = *itr;
                cacheSet.erase(itr);
                cacheSet.emplace(cacheSet.end(), block); // Put the invalid entry at the end of set(For implementation reasons)
				busInvalidations++;
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
				itr->first.second = 0;
				break;
			}
		}
		return;
	}
	
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
	AddressInfo ai = getAddressInfo(address);
	funcPointer fp = std::bind(&CacheController::onBusresponse, *this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	// Check if the instruction is a read or write instruction
	if(isWrite)
		writeOnCache(ai.setIndex, ai.tag, cache.at(ai.setIndex), response, config, mutex, convar, Bus, threadId, fp);
	else{
		readFromCache(ai.setIndex, ai.tag, cache.at(ai.setIndex), response, config, mutex, convar, Bus, threadId, fp, &writebacks);
		totalReads++;
	}
	
	globalCycles += response -> cycles;
	
	if(response -> hit == 0)
		globalMisses++;
	else
		globalHits++;
	
	if(response -> eviction != 0)
		globalEvictions++;

    totalInstructions++;
    dataTrafficBytes += response->trafficBytes; // Assuming trafficBytes is part of CacheResponse
}



void CacheController::runTracefile(std::mutex& mutex, std::condition_variable& convar, bus& Bus) 
{
	// process each input line
	std::string line;
	std::regex readPattern("R (0x[[:xdigit:]]+)");
	std::regex writePattern("W (0x[[:xdigit:]]+)");

	std::ofstream outfile(outputFile);
	std::ifstream infile(inputFile);
	
	while (getline(infile, line)) {
		// these strings will be used in the file output
		std::string opString, activityString;
		std::smatch match; // will eventually hold the hexadecimal address string
		unsigned long int address;
		// create a struct to track cache response
		CacheResponse response;
		
		if (std::regex_match(line, match, readPattern)) {
			std::istringstream hexStream(match.str(1));
			hexStream >> std::hex >> address;
			outfile << "R " << match.str(1);
			cacheAccess(&response, false, address, mutex, convar, Bus);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		} else if (std::regex_match(line, match, writePattern)) {
			std::istringstream hexStream(match.str(1));
			hexStream >> std::hex >> address;
			outfile << "W " << match.str(1);
			cacheAccess(&response, true, address, mutex, convar, Bus);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		} 
		else {
			throw std::runtime_error("Encountered unknown line format in tracefile.");
		}
		outfile << std::endl;
	}

	// Final cache statistics
	outfile << "Hits: " << globalHits << " Misses: " << globalMisses << " Evictions: " << globalEvictions << std::endl;
	outfile << "Cycles: " << globalCycles << std::endl;

	infile.close();
	outfile.close();
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
    stats << "Idle Cycles: " << globalCycles - totalInstructions << "\n";
    stats << "Cache Misses: " << globalMisses << "\n";
    stats << "Cache Miss Rate: " << cacheMissRate << "%\n";
    stats << "Cache Evictions: " << globalEvictions << "\n";
    stats << "Writebacks: " << writebacks << "\n";
    stats << "Bus Invalidations: " << busInvalidations << "\n";
    stats << "Data Traffic (Bytes): " << dataTrafficBytes << "\n\n";

    return stats.str();
}
