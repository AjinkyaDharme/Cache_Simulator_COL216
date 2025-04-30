
#ifndef _CACHECONF_H_
#define _CACHECONF_H_


enum class ReplacementPolicy 
{
	LRU
};

enum class WritePolicy 
{
	WriteBack
};

enum class CoherenceProtocol 
{
	MESI
};

// structure to hold information from the configuration file
struct ConfigInfo 
{
	unsigned int numberIndicies; 		// how many sets are in the cache
	unsigned int blockSize; 			// size of each block in bytes
	unsigned int associativity; 		// the level of associativity (N)
	ReplacementPolicy replacementPolicy;
	WritePolicy writePolicy;
	unsigned int cacheAccessCycles;
	unsigned int memoryAccessCycles;
	CoherenceProtocol protocol;
};

#endif //CACHECONF
