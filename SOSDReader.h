#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Shared reader for SOSD binary key files (role 1: data reading).
//
// File layout (little-endian):
//   [uint64 key count n][key 0][key 1]...[key n-1]
// The header stores only the count; the key width is implied by the file name
// (*_uint32 / *_uint64). This project targets the uint32 datasets
// (e.g. books_200M_uint32), so the constructor verifies that the file size is
// exactly 8 + n * 4 bytes and rejects anything else (truncated files, uint64
// datasets, non-SOSD files).
//
// Keys stay on disk: readKey() fetches one key by position and readBlock()
// fetches a contiguous run, so memory use is bounded by what the caller asks for.
class SOSDReader {
public:
	static constexpr uint64_t kHeaderBytes = 8;
	static constexpr uint32_t kKeyBytes = 4;

	explicit SOSDReader(const std::string& path);
	~SOSDReader();

	SOSDReader(const SOSDReader&) = delete;
	SOSDReader& operator=(const SOSDReader&) = delete;

	// Header information.
	uint64_t size() const { return n_; }              // number of keys
	uint32_t keySize() const { return kKeyBytes; }    // bytes per key
	uint64_t headerSize() const { return dataOffset_; }
	uint64_t fileSize() const { return fileSize_; }   // bytes on disk
	const std::string& path() const { return path_; }

	// Key at position `index` (0-based). Throws std::out_of_range if
	// index >= size(), std::runtime_error if the read fails.
	uint64_t readKey(uint64_t index) const;

	// Keys [start, start + count) into `out`, clipped at the end of the file
	// (out.size() tells how many were read; empty if start >= size()).
	// Throws std::runtime_error if the read fails.
	void readBlock(uint64_t start, uint32_t count,
					std::vector<uint32_t>& out) const;

private:
	mutable std::ifstream file_;
	std::string path_;
	uint64_t n_ = 0;
	uint64_t fileSize_ = 0;
	uint64_t dataOffset_ = kHeaderBytes;
};
