#pragma once
#include "SearchInterface.h"

class BlockIndex : public Searcher { 
public: 
	BlockIndex(SOSDReader& reader, uint32_t blockSize); 
	void build() override; 
	uint64_t find(uint32_t key) override; 
	const char* name() const override { return "Block index"; } 
private: 
	SOSDReader& reader_; 
	uint32_t blockSize_; 
	std::vector<uint32_t> blockMax_; 
};
