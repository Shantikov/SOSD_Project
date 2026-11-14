#include "SOSDReader.h"
#include <algorithm>

SOSDReader::SOSDReader(const std::string& path) { 
	file_.open(path, std::ios::binary); 
	if (!file_) throw std::runtime_error("cannot open file"); 
	uint64_t preamble = 0; 
	file_.read(reinterpret_cast<char*>(&preamble), 8); 
	n_ = preamble; 
}

SOSDReader::~SOSDReader() { file_.close(); }

uint64_t SOSDReader::readKey(uint64_t index) const { 
	if (index >= n_) throw std::out_of_range("index out of range"); 
	file_.seekg(dataOffset_ + index * 4, std::ios::beg); 
	uint32_t k = 0; 
	file_.read(reinterpret_cast<char*>(&k), 4); 
	return k; 
}

void SOSDReader::readBlock(uint64_t start, uint32_t count, 
							std::vector<uint32_t>& out) const { 
	if (start >= n_) { out.clear(); return; } 
	uint32_t actual = static_cast<uint32_t>(
		std::min<uint64_t>(count, n_ - start)); 
	out.resize(actual);
	const auto bytes = static_cast<std::streamsize>(actual) *
		static_cast<std::streamsize>(sizeof(uint32_t));
	if (!file_.seekg(dataOffset_ + start * 4, std::ios::beg) ||
		!file_.read(reinterpret_cast<char*>(out.data()), bytes)) {
		out.clear();
		throw std::runtime_error("cannot read complete SOSD block");
	}
}
