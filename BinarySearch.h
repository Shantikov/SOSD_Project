#pragma once
#include "SearchInterface.h"

class BinarySearch : public Searcher { 
public: 
	explicit BinarySearch(SOSDReader& reader) : reader_(reader) {} 
	void build() override {} 
	uint64_t find(uint32_t key) override; 
	const char* name() const override { return "Full binary search"; } 
private: 
	SOSDReader& reader_; 
};
