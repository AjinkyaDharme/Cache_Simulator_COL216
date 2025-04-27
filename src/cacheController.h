#ifndef _CACHECONTROLLER_H_
#define _CACHECONTROLLER_H_

#include <string>
#include <mutex>
#include <condition_variable>

#include "cacheconf.h"
#include "cacheStructure.h"
#include "busOperation.h"


// this structure is filled with information about each memory access
struct CacheResponse 
{
	bool hit; 				// did this memory operation encounter a hit?
	bool eviction;			// did this memory operation involve an eviction?
	bool dirtyEviction; 	// was the evicted block marked as dirty?
	unsigned int cycles; 	// how many clock cycles did this particular operation take?
};

class CacheController
{
	private:
		struct AddressInfo 
        {
			unsigned long int tag;
			unsigned int setIndex;
		};

		ConfigInfo config;
		std::string inputFile, outputFile;

		unsigned int numByteOffsetBits;
		unsigned int numIndexBits;

		unsigned int globalCycles;
		unsigned int globalHits;
		unsigned int globalMisses;
		unsigned int globalEvictions;
		int threadId;
		
		funcPointer fp;
		typedef std::function <void(unsigned int, unsigned long int, std::string)> funcPointer;
        
        std::vector <std::vector <cacheEntry> > cache;

		// function to allow read or write access to the cache
		void cacheAccess(CacheResponse*, bool, unsigned long int, std::mutex&, std::condition_variable&, bus&);
		// function that can compute the index and tag matching a specific address
		AddressInfo getAddressInfo(unsigned long int);
		// compute the number of clock cycles used to complete a memory access
		void updateCycles(CacheResponse*, bool);

	public:
		CacheController(ConfigInfo, char *, int);
		void runTracefile(std::mutex&, std::condition_variable&, bus&);
		void onBusresponse(unsigned int, unsigned long int, std::string);
};

#endif //CACHECONTROLLER