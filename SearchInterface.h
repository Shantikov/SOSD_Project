#pragma once
#include <cstdint>
#include <vector>
#include "SOSDReader.h"

static const uint64_t NOT_FOUND = UINT64_MAX;

class Searcher { 
public: 
	virtual ~Searcher() = default; 
	virtual uint64_t find(uint32_t key) = 0; 
	virtual void build() {} 
	virtual const char* name() const = 0; 
};
