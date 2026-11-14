#include "BinarySearch.h"

uint64_t BinarySearch::find(uint32_t key) { 
	uint64_t lo = 0, hi = reader_.size(); 
	while (lo < hi) { 
		uint64_t mid = lo + (hi - lo) / 2; 
		uint32_t v = static_cast<uint32_t>(reader_.readKey(mid)); 
		if (v < key) lo = mid + 1; 
		else hi = mid; 
	} 
	if (lo < reader_.size() && reader_.readKey(lo) == key) return lo; 
	return NOT_FOUND; 
}
