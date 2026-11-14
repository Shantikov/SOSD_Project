#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

class SOSDReader { 
public: 
	explicit SOSDReader(const std::string& path); 
	~SOSDReader(); 
	
	uint64_t size() const { return n_; } 
	uint64_t readKey(uint64_t index) const; 
	void readBlock(uint64_t start, uint32_t count, 
					std::vector<uint32_t>& out) const; 

private: 
	mutable std::ifstream file_; 
	uint64_t n_ = 0; 
	uint64_t dataOffset_ = 8; 
};
