#ifndef _CACHEACCESS_H_
#define _CACHEACCESS_H_


#include <string>
#include <mutex>
#include <condition_variable>

#include "cacheController.h"
#include "cacheStructure.h"
#include "cacheconf.h"
#include "busOperation.h"

// void writeOnCache(unsigned int, unsigned long int, std::vector <std::vector <cacheEntry> >, CacheResponse*, ConfigInfo);
// void readFromCache(unsigned int, unsigned long int, std::vector <std::vector <cacheEntry> >, CacheResponse*, ConfigInfo);
unsigned long int writeOnCache(unsigned int, unsigned long int, std::vector <cacheEntry>&, CacheResponse*, ConfigInfo, std::mutex&, std::condition_variable&, bus&, int, std::function<void(unsigned int, unsigned long int, std::string)>);
unsigned long int readFromCache(unsigned int, unsigned long int, std::vector <cacheEntry>&, CacheResponse*, ConfigInfo, std::mutex&, std::condition_variable&, bus&, int, std::function<void(unsigned int, unsigned long int, std::string)>);
unsigned long int accessBus(unsigned int, unsigned long int, std::mutex&, std::condition_variable&, bus&, std::string, int, std::function<void(unsigned int, unsigned long int, std::string)>,unsigned long int, unsigned long* ); 

extern unsigned long int totalBusTransactions;
#endif //CACHEACCESS