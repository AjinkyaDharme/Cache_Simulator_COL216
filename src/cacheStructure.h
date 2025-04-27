#ifndef _CACHESTRUCTURE_H_
#define _CACHESTRUCTURE_H_


#include <vector>
#include <utility>
#include <list>

// nonDataBits.first represents Valid Bits
// nonDataBits.second represents Dirty Bits
typedef std::pair <bool, bool> nonDataBits;


typedef std::pair <nonDataBits, bool> referenceBits;

// cacheEntry.second represent Tag Bits
typedef std::pair <referenceBits, unsigned long int> cacheEntry;

#endif //CACHESTRUCTURE