#include "BlockIndex.h"
#include <algorithm>
#include <cstddef>
#include <stdexcept>

BlockIndex::BlockIndex(SOSDReader& reader, uint32_t blockSize)
	: reader_(reader), blockSize_(blockSize) {
	if (blockSize_ == 0) {
		throw std::invalid_argument("block size must be positive");
	}
}

void BlockIndex::build() {
	const uint64_t keyCount = reader_.size();
	const uint64_t blockCount = keyCount / blockSize_ + (keyCount % blockSize_ != 0);
	const uint64_t bufferKeys = std::min<uint64_t>(blockSize_, keyCount);

	// Reject configurations whose index and buffer alone exceed the process limit.
	constexpr uint64_t maxKeysInMemory = 512'000'000 / sizeof(uint32_t);
	if (bufferKeys > maxKeysInMemory || blockCount > maxKeysInMemory - bufferKeys) {
		throw std::length_error("block index and buffer exceed 512 MB");
	}

	blockMax_.reserve(static_cast<std::size_t>(blockCount));
	blockBuffer_.reserve(static_cast<std::size_t>(bufferKeys));
	blockMax_.clear();

	try {
		for (uint64_t block = 0; block < blockCount; ++block) {
			const uint64_t start = block * blockSize_;
			const uint64_t count = std::min<uint64_t>(blockSize_, keyCount - start);
			blockMax_.push_back(static_cast<uint32_t>(reader_.readKey(start + count - 1)));
		}
	} catch (...) {
		blockMax_.clear();
		throw;
	}
}

uint64_t BlockIndex::find(uint32_t key) {
	// The earliest candidate block gives the first occurrence across duplicates.
	const auto candidate = std::lower_bound(blockMax_.begin(), blockMax_.end(), key);
	if (candidate == blockMax_.end()) {
		return NOT_FOUND;
	}

	const uint64_t block = static_cast<uint64_t>(candidate - blockMax_.begin());
	const uint64_t start = block * blockSize_;
	reader_.readBlock(start, blockSize_, blockBuffer_);
	const auto found = std::lower_bound(blockBuffer_.begin(), blockBuffer_.end(), key);
	if (found == blockBuffer_.end() || *found != key) {
		return NOT_FOUND;
	}
	return start + static_cast<uint64_t>(found - blockBuffer_.begin());
}
