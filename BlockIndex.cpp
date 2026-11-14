#include "BlockIndex.h"
#include <algorithm>

BlockIndex::BlockIndex(SOSDReader& reader, uint32_t blockSize) 
	: reader_(reader), blockSize_(blockSize) {}

void BlockIndex::build() { 
	blockMax_.clear(); 
	uint64_t n = reader_.size(); 
	for (uint64_t start = 0; start < n; start += blockSize_) { 
		uint64_t last = std::min<uint64_t>(start + blockSize_, n) - 1; 
		blockMax_.push_back(static_cast<uint32_t>(reader_.readKey(last))); 
	} 
}

uint64_t BlockIndex::find(uint32_t key) { 
	if (blockMax_.empty()) return NOT_FOUND; 
	auto it = std::lower_bound(blockMax_.begin(), blockMax_.end(), key); 
	if (it == blockMax_.end()) return NOT_FOUND; 
	uint64_t block = static_cast<uint64_t>(it - blockMax_.begin()); 
	uint64_t start = block * blockSize_; 
	uint64_t end = std::min<uint64_t>(start + blockSize_, reader_.size()); 
	
	std::vector<uint32_t> buf; 
	reader_.readBlock(start, static_cast<uint32_t>(end - start), buf); 
	auto vit = std::lower_bound(buf.begin(), buf.end(), key); 
	if (vit != buf.end() && *vit == key) 
		return start + static_cast<uint64_t>(vit - buf.begin()); 
	return NOT_FOUND; 
}
