#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>


#include "cacheconf.h"
#include "cacheController.h"
#include "busOperation.h"

void bus::putMsg_onBus(std::string address, std::string message, funcPointer sendResponse, int threadId, unsigned long int &cycles)
{
    // If the address is already present in the bus, send the message to the function pointers of cache controllers present in the value vector
    if (this->bus.find(address) != this->bus.end())
    {
        // element contains address to the vector<pair<funcPointer, threadId> >
        auto element = this->bus.find(address);

        if (message == "write")
        {
            // if no other entry for the same thread is present, push_back the new entry
            auto newValue = std::make_pair(sendResponse, threadId);
            int foundThreadid = -1;

            for (auto itr = element->second.begin(); itr != element->second.end(); itr++)
            {
                if (itr->second == threadId)
                {
                    foundThreadid = 0;
                    break;
                }
            }
            if (foundThreadid == -1)
            {
                element->second.push_back(newValue); // Add current thread's functionPointer in the vector
            }

            // Sort the vector by threadId to prioritize the lowest-numbered core
            std::sort(element->second.begin(), element->second.end(),
                      [](const std::pair<funcPointer, int> &a, const std::pair<funcPointer, int> &b) {
                          return a.second < b.second; // Sort by threadId
                      });

            for (auto itr = element->second.begin(); itr != element->second.end();)
            { // Iterate the vector to send response to the other threads

                if (itr->second != threadId) // This will make sure that the thread does not invalidate its own cache entry
                {
                    // Convert the string key: "index|tag" to unsigned int index and unsigned long int tag
                    int pos = address.find("|");
                    unsigned int index = std::stoi(address.substr(0, pos - 0));
                    unsigned long int tag = std::stoi(address.substr(pos + 1, address.length() - pos - 1));

                    funcPointer fp = itr->first;
                    fp(index, tag, message);
                    itr = element->second.erase(itr); // Delete the functionPointer from the vector, because the corresponding cache does not
                                                      // contain the entry for the address anymore.
                                                      // erase() shifts the next elements to the left, and hence erase() will return the value to the next element
                }
                else
                {
                    itr++;
                }
            }

            if (element->second.size() == 1)
            {
                // Convert the string key: "index|tag" to unsigned int index and unsigned long int tag
                int pos = address.find("|");
                unsigned int index = std::stoi(address.substr(0, pos - 0));
                unsigned long int tag = std::stoi(address.substr(pos + 1, address.length() - pos - 1));
                auto itr = element->second.begin();
                funcPointer fp = itr->first;
                fp(index, tag, "notSharedAnymore");
            }
            return;
        }

        if (message == "read")
        {
            // if no other entry for the same thread is present, push_back the new entry
            auto newValue = std::make_pair(sendResponse, threadId);
            int foundThreadid = -1;

            for (auto itr = element->second.begin(); itr != element->second.end(); itr++)
            {
                if (itr->second == threadId)
                {
                    foundThreadid = 0;
                    break;
                }
            }
            if (foundThreadid == -1)
            {
                element->second.push_back(newValue); // Add current thread's functionPointer in the vector
            }

            // Sort the vector by threadId to prioritize the lowest-numbered core
            std::sort(element->second.begin(), element->second.end(),
                      [](const std::pair<funcPointer, int> &a, const std::pair<funcPointer, int> &b) {
                          return a.second < b.second; // Sort by threadId
                      });

            // SHARED STATE LOGIC
            if (element->second.size() > 1)
            {
                // If size = 1, this means that only the current thread's function pointer is present in the vector and no other
                // threads(caches) share the same data. Thus, set the shared bit only when there is sharing of data
                for (auto itr = element->second.begin(); itr != element->second.end(); itr++)
                { // Iterate the vector to send response to the other threads to make their state shared

                    // Convert the string key: "index|tag" to unsigned int index and unsigned long int tag
                    int pos = address.find("|");
                    unsigned int index = std::stoi(address.substr(0, pos - 0));
                    unsigned long int tag = std::stoi(address.substr(pos + 1, address.length() - pos - 1));

                    funcPointer fp = itr->first;
                    fp(index, tag, message);
                }
            }

            // Snooping Block was in modified state, and now it is in shared state
            if (element->second.size() == 2)
            {
                // cycles=1+(100+2*N);
            }
            // Snooping Block was already in shared state, and now it is in shared state
            if (element->second.size() > 2)
            {
                // cycles=1+2*N;
            }
            return;
        }
    }

    // EXCLUSIVE STATE LOGIC for first-time access
    else
    {
        std::cout << "First time access to the address: " << address << std::endl;
        cycles += 100;
        std::vector<std::pair<funcPointer, int>> vec;
        vec.push_back(std::make_pair(sendResponse, threadId));
        this->bus[address] = vec;
    }
}