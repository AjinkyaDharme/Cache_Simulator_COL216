#include <iostream>
#include <utility>
#include <list>
#include <thread>
#include <cmath>


#include "cacheController.h"
#include "cacheStructure.h"
#include "cacheAccess.h"
#include "busOperation.h"

extern CacheController** controllers;
extern int numThreads;
unsigned long int totalBusTransactions = 0;
unsigned long int accessBus(unsigned int indexNo, unsigned long int tag, std::mutex &mutex, std::condition_variable& convar,
            bus &Bus, std::string message, int threadId, funcPointer fp, unsigned long int blockSize, unsigned long int &writebacks)
{   

    std::lock_guard<std::mutex> lock(mutex);
    unsigned long int cycles=0;
    std::string address = std::to_string(indexNo) + "|" + std::to_string(tag);
    totalBusTransactions++;
    Bus.putMsg_onBus(address, message, fp, threadId,cycles,blockSize,writebacks);

    return cycles;
}
// Update other cores' idle cycles when a core is executing instructions
void updateOtherCoresIdleCycles(int currentThreadId, unsigned int cycles) {
    for (int i = 0; i < numThreads; i++) {
        if (i != currentThreadId) {
            controllers[i]->incrementIdleCycles(cycles);
        }
    }
}
unsigned long int writeOnCache(unsigned int indexNo, unsigned long int tag, std::vector <cacheEntry>& cacheSet, CacheResponse* response,
                    ConfigInfo config, std::mutex &mutex, std::condition_variable &convar, bus &Bus, int threadId, funcPointer fp)
{
    int flag = -1; // This will keep check whether the value has been written in the cache or not
    unsigned long int numberOfCycles = 0;
    unsigned long int writebacks = 0;
    
    // The cache will always be accessed for the first time. Add the cache access time to the total cycles
    numberOfCycles += config.cacheAccessCycles;
    updateOtherCoresIdleCycles(threadId,config.cacheAccessCycles);
    
    for(auto itr = cacheSet.begin(); itr != cacheSet.end(); itr++)
    {// We are at the index where we have to perform write operation, now check every set to find good position to write the data
       
        // CHECK FOR VALID BIT
        if(itr -> first.first.first == 0)
        {// If valid bit is 0, simply write on the cache block
            
            response -> hit = 0;
            response -> eviction = 0;
            response -> dirtyEviction = 0;
           
            numberOfCycles+=accessBus(indexNo, tag, mutex, convar, Bus, "write", threadId, fp, config.blockSize, writebacks);
            itr -> first.first.first = 1;
            flag = 0;
            
            
            //The write-policy is write-back, we set the dirty bit
            itr -> second = tag;
            itr -> first.first.second = 1; //dirty bit set which tells that the data changed to MODIFIED state
            
            if (config.replacementPolicy == ReplacementPolicy::LRU) 
            {// move the recently written block to the first position of the array
                cacheEntry block = *itr;
                cacheSet.erase(itr);
                cacheSet.emplace(cacheSet.begin(), block);
            }
            break;
        }
        
        // CHECK FOR TAG BITS
        if(itr -> second == tag && itr -> first.first.first == 1)
        {// If valid bit is 1 and we find the matching tag
            response -> hit = 1;
            response -> eviction = 0;
            response -> dirtyEviction = 0;
            
            if(itr -> first.second == 1)
            {   
                std::string address = std::to_string(indexNo) + "|" + std::to_string(tag);
                
                // Lock the bus to safely access and modify its data structures
                std::unique_lock<std::mutex> lock(mutex);
                
                // Find all caches that have this block
                auto element = Bus.bus.find(address);
                if(element != Bus.bus.end()) {
                    // Iterate through all caches that have this block
                    for(auto mapItr = element->second.begin(); mapItr != element->second.end();) {
                        if(mapItr->second != threadId) {
                            // Call the other cache's onBusresponse with invalidate message
                            funcPointer otherFp = mapItr->first;
                            otherFp(indexNo, tag, "notSharedAnymore");
                            
                            // Remove this entry as it's no longer valid
                            mapItr = element->second.erase(mapItr);
                        } else {
                            ++mapItr;
                        }
                    }
        }
        
        // Reset shared bit 
        itr->first.second = 0;
        totalBusTransactions++;

                
            }

            flag = 0;
            
            //The write-policy is write-back, we set the dirty bit, since the data is not written back to 
            // the memory, no clock cycles will be added.
            itr -> first.first.second = 1;  //dirty bit set which tells that the data changed to MODIFIED state
            

            if (config.replacementPolicy == ReplacementPolicy::LRU) 
            {// move the recently written block to the first position of the array
                cacheEntry block = *itr;
                cacheSet.erase(itr);
                cacheSet.emplace(cacheSet.begin(), block);
            }
            break;
        }
    }
    if(flag == -1)
    {// Nothing matched, we need to perform eviction
        response->hit = 0;
        response->eviction = 1;

        // Add memory access cycles
        numberOfCycles += accessBus(indexNo, tag, mutex, convar, Bus, "write", threadId, fp, config.blockSize, writebacks);

        auto itr = std::prev(cacheSet.end());

        if (itr->first.first.second == 1) // Dirty eviction
        {
            response->dirtyEviction = 1;
            numberOfCycles += config.memoryAccessCycles;
            writebacks++;

            // Add block size to trafficBytes for dirty eviction
            response->trafficBytes += config.blockSize;
        }
        else
        {
            response->dirtyEviction = 0;
        }

        cacheSet.erase(itr);
        cacheEntry block;
        block.first.first.first = 1;
        block.first.first.second = 1; // Dirty bit set
        block.second = tag;

        cacheSet.emplace(cacheSet.begin(), block);
    }

   
    response -> cycles = numberOfCycles;
    return writebacks;
}

unsigned long int readFromCache(unsigned int indexNo, unsigned long int tag, std::vector <cacheEntry>& cacheSet, CacheResponse* response, ConfigInfo config, 
                     std::mutex &mutex, std::condition_variable &convar, bus &Bus, int threadId, funcPointer fp)
{
    
    unsigned long int numberOfCycles = 0;
    unsigned long int writebacks = 0;
    int flag = -1;
    
    // The cache will always be accessed for the first time. Add the cache access time to the total cycles
    numberOfCycles += config.cacheAccessCycles;
    updateOtherCoresIdleCycles(threadId,config.cacheAccessCycles);

    // We are at the index where we have to perform read operation, now check every set to find where the data might be present to read from.
    for(auto itr = cacheSet.begin(); itr != cacheSet.end(); itr++)
    {
       
        // CHECK FOR VALID BIT
        if(itr -> first.first.first == 0)
        {
            // If valid bit is 0, simply write on the cache block
            response -> hit = 0;
            response -> eviction = 0;
            response -> dirtyEviction = 0;
            flag = 0;

            itr -> first.first.first = 1;  // Put Valid bit to 1
            itr -> second = tag;
            
            //put request on bus to fetch from cache if not then we have to fetch from memory
            numberOfCycles+=accessBus(indexNo, tag, mutex, convar, Bus, "read", threadId, fp,config.blockSize,writebacks);

            // move the recently written block to the first position of the array
            if (config.replacementPolicy == ReplacementPolicy::LRU) 
            {
                cacheEntry block = *itr;
                cacheSet.erase(itr);
                cacheSet.emplace(cacheSet.begin(), block);
            }
            break;
        }
        
        // Local Read hit
        if(itr -> second == tag && itr -> first.first.first == 1)
        {// If valid bit is 1 and we find the matching tag
            response -> hit = 1;
            response -> eviction = 0;
            response -> dirtyEviction = 0;

            flag = 0;
            
            if (config.replacementPolicy == ReplacementPolicy::LRU) 
            {// move the recently written block to the first position of the array
                cacheEntry block = *itr;
                cacheSet.erase(itr);
                cacheSet.emplace(cacheSet.begin(), block);
            }
            break;
        }
    }

    if (flag == -1) // Cacheset is full need eviction!
    {
        response->hit = 0;
        response->eviction = 1;

        
        numberOfCycles += accessBus(indexNo, tag, mutex, convar, Bus, "read", threadId, fp, config.blockSize,writebacks);

        auto itr = std::prev(cacheSet.end());

        if (config.writePolicy == WritePolicy::WriteBack)
        {
            if (itr->first.first.second == 1) // Dirty eviction
            {
                response->dirtyEviction = 1;
                numberOfCycles += config.memoryAccessCycles;
                writebacks++;

                // Add block size to trafficBytes for dirty eviction
                response->trafficBytes += config.blockSize;
            }
        }

        cacheSet.erase(itr);
        cacheEntry block;
        //Chaneg of state to EXCLUSIVE
        block.first.first.first = 1;
        block.first.first.second = 0;
        block.second = tag;

        cacheSet.emplace(cacheSet.begin(), block);
    }

    
    response -> cycles = numberOfCycles;
    return writebacks;
}