#ifndef _BUSOPERATION_H
#define _BUSOPERATION_H

#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <functional>


#include "cacheconf.h"
#include "cacheStructure.h"

typedef std::function <void(unsigned int, unsigned long int, std::string)> funcPointer;
typedef std::vector <std::pair <funcPointer, int> > value;
class bus
{
    public:
        std::map <std::string, value> bus;
        void putMsg_onBus(std::string, std::string, std::function<void(unsigned int, unsigned long int, std::string)>, int);
        
};

#endif //BUSOPERATION