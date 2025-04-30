#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>

#include "cacheconf.h"
#include "cacheController.h"
#include "busOperation.h"

// Assume global access to CacheController objects.
extern CacheController** controllers;  // defined in main.cpp
extern int numThreads;

void bus::putMsg_onBus(std::string address, std::string message, funcPointer sendResponse, int threadId,
                       unsigned long int &cycles, unsigned long int blockSize, unsigned long int &writebacks)
{
    // If the address is already present in the bus, send the message to the function pointers of cache controllers present in the value vector
    if(this->bus.find(address) != this->bus.end())
    {
        // element contains address to the vector<pair<funcPointer, threadId> >
        auto element = this->bus.find(address);

        if(message == "write")
        {   
            // if no other entry for same thread is present push_back the new entry
            auto newValue = std::make_pair(sendResponse, threadId);
            int foundThreadid = -1;
            
            for(auto itr = element->second.begin(); itr != element->second.end(); itr++)
            {
                if(itr->second == threadId)
                {
                    foundThreadid = 0;
                    break;
                }
            }
            if(foundThreadid == -1)
            {
                element->second.push_back(newValue);   // Add current thread's functionPointer in the vector
            }


            // std::sort(element->second.begin(), element->second.end(),
            // [](const std::pair<funcPointer, int> &a, const std::pair<funcPointer, int> &b) {
            //     return a.second < b.second; // Sort by threadId
            // });

            //------------Here check if the snooping cache is in E/S state if so then add 100 cycles of memory 
            // and if in modified state then add 100+100 cycles.
            // ...-->INVALIDATE STATE
            if(element->second.size()>1){
                controllers[threadId]->incrementBusInvalidations(); // Track invalidations
            }
            unsigned int addedCycles = 0;
            for(auto itr = element->second.begin(); itr != element->second.end(); )
            {   // Iterate the vector to send response to the other threads holding this address.
                if(itr->second != threadId)   // ensure that the thread does not invalidate its own cache entry
                {   
                    // convert the string key "index|tag" to unsigned int index and unsigned long int tag
                    int pos = address.find("|");
                    unsigned int index = std::stoi(address.substr(0, pos));
                    unsigned long int tag = std::stoul(address.substr(pos + 1));
                    
                    CacheController::LineState state = controllers[itr->second]->getLineState(index, tag);
                    if(state == CacheController::LineState::Shared || state == CacheController::LineState::Exclusive)
                        {
                            addedCycles += 100;
                            cycles += addedCycles; // cache transfer penalty
                            controllers[threadId]->addTrafficBytes(blockSize);
                            
                        }
                        else if(state == CacheController::LineState::Modified) {
                            addedCycles += 200;
                            cycles += addedCycles; // cache transfer penalty
                            controllers[threadId]->addTrafficBytes(blockSize); // Track traffic for writebacks
                            writebacks++;
                            controllers[threadId]->addTrafficBytes(blockSize);
                    }
                    
                    funcPointer fp = itr->first;
                    fp(index, tag, message);
                    itr = element->second.erase(itr); // erase the entry because the corresponding cache no longer has the correct entry
                    
                } 
                else
                {
                    itr++;
                }  
            }
            
            
            for (int i = 0; i < 4; i++) {  // Assuming 4 cores
                if (i != threadId) {
                    controllers[i]->incrementIdleCycles(addedCycles);
                }
            }

            if(element->second.size() == 1)
            {	
                // convert the string key: "index|tag" to index and tag
                int pos = address.find("|");
                unsigned int index = std::stoi(address.substr(0, pos));
                unsigned long int tag = std::stoul(address.substr(pos + 1));
                auto itr = element->second.begin();
                funcPointer fp = itr->first;
                fp(index, tag, "notSharedAnymore");
            }
           
            return;
        }
        
        if(message == "read")
        {
            // if no other entry for same thread is present push_back the new entry
            auto newValue = std::make_pair(sendResponse, threadId);
            int foundThreadid = -1;
           
            for(auto itr = element->second.begin(); itr != element->second.end(); itr++)
            {
                if(itr->second == threadId)
                {
                    foundThreadid = 0;
                    break;
                }
            }
            if(foundThreadid == -1)
            {   
                element->second.push_back(newValue);   // Add current thread's functionPointer in the vector
            }
            
            
            // std::sort(element->second.begin(), element->second.end(),
            //           [](const std::pair<funcPointer, int> &a, const std::pair<funcPointer, int> &b) {
                //               return a.second < b.second; // Sort by threadId
                //           });
                unsigned int addedCycles = 0;
                //-----------I have to handle modified state logic here to add extra 100 cycles for memory writeback----------
                if(element->second.size() > 1)
                { 
                    addedCycles += 2 * (blockSize / 4);   // cache transfer penalty
                    cycles+=addedCycles;
                    controllers[threadId]->addTrafficBytes(blockSize); // Track traffic for cache-to-cache transfer
                    
                    // For each snooping cache, check its line status.
                    for(auto itr = element->second.begin(); itr != element->second.end(); itr++)
                    {
                        int pos = address.find("|");
                        unsigned int index = std::stoi(address.substr(0, pos));
                        unsigned long int tag = std::stoul(address.substr(pos + 1));
                        CacheController::LineState state = controllers[itr->second]->getLineState(index, tag);
                        if(state == CacheController::LineState::Modified)
                        {
                            cycles += 100;  // add extra penalty for memory writeback from modified state
                            writebacks++;
                            controllers[threadId]->addTrafficBytes(blockSize); // Track traffic for writebacks
                        }
                    }

                    
                    for (int i = 0; i < 4; i++) {  // Assuming 4 cores
                        if (i != threadId) {
                            controllers[i]->incrementIdleCycles(addedCycles);
                        }
                    }
                    
                    // Send response to all snooping caches to set their state to shared
                    // Change from EXCLUSIVE/MODIFIED/SHARED to SHARED.
                    for(auto itr = element->second.begin(); itr != element->second.end(); itr++)
                    {   
                        int pos = address.find("|");
                        unsigned int index = std::stoi(address.substr(0, pos));
                        unsigned long int tag = std::stoul(address.substr(pos + 1));
                        funcPointer fp = itr->first;
                        fp(index, tag, message);
                    }
            }
 
            return;
        }
    }
    
    // EXCLUSIVE state logic for READ and MODIFIED for WRITE
    else
    {   
        controllers[threadId]->addTrafficBytes(blockSize); // Track traffic for memory access
        std::vector<std::pair<funcPointer, int>> vec;
        vec.push_back(std::make_pair(sendResponse, threadId));
        this->bus[address] = vec;

        unsigned int addedCycles=  100;
        cycles += addedCycles;  // memory access penalty
        for (int i = 0; i < 4; i++) {  // Assuming 4 cores
            if (i != threadId) {
                controllers[i]->incrementIdleCycles(addedCycles);
            }
        }
    }
}