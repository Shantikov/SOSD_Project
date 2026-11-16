#include "SOSDReader.h"
#include <algorithm>

SOSDReader::SOSDReader(const std::string& path) : path_(path) {
	file_.open(path, std::ios::binary);
	if (!file_) throw std::runtime_error("cannot open file: " + path);

	file_.seekg(0, std::ios::end);
	const std::streamoff end = file_.tellg();
	if (end < 0) throw std::runtime_error("cannot determine file size: " + path);
	fileSize_ = static_cast<uint64_t>(end);
	if (fileSize_ < kHeaderBytes) {
		throw std::runtime_error("SOSD header missing (file shorter than 8 bytes): " + path);
	}

	uint64_t count = 0;
	file_.seekg(0, std::ios::beg);
	if (!file_.read(reinterpret_cast<char*>(&count), kHeaderBytes)) {
		throw std::runtime_error("cannot read SOSD header: " + path);
	}

	// The header has no key-width field, so check it against the payload size.
	const uint64_t payload = fileSize_ - kHeaderBytes;
	if (count > payload / kKeyBytes || count * kKeyBytes != payload) {
		std::string why = "SOSD header does not match file size (header n=" +
			std::to_string(count) + ", payload=" + std::to_string(payload) +
			" bytes, expected n*4)";
		if (count <= payload / 8 && count * 8 == payload && count != 0) {
			why += "; this looks like a uint64 dataset, a uint32 one is required";
		}
		throw std::runtime_error(why + ": " + path);
	}
	n_ = count;
}

SOSDReader::~SOSDReader() { file_.close(); }

uint64_t SOSDReader::readKey(uint64_t index) const {
	if (index >= n_) throw std::out_of_range("index out of range");
	uint32_t k = 0;
	file_.clear();
	if (!file_.seekg(static_cast<std::streamoff>(dataOffset_ + index * kKeyBytes), std::ios::beg) ||
		!file_.read(reinterpret_cast<char*>(&k), kKeyBytes)) {
		throw std::runtime_error("cannot read SOSD key");
	}
	return k;
}

void SOSDReader::readBlock(uint64_t start, uint32_t count,
							std::vector<uint32_t>& out) const {
	if (start >= n_) { out.clear(); return; }
	uint32_t actual = static_cast<uint32_t>(
		std::min<uint64_t>(count, n_ - start));
	out.resize(actual);
	if (actual == 0) return;
	const auto bytes = static_cast<std::streamsize>(actual) *
		static_cast<std::streamsize>(sizeof(uint32_t));
	file_.clear();
	if (!file_.seekg(static_cast<std::streamoff>(dataOffset_ + start * kKeyBytes), std::ios::beg) ||
		!file_.read(reinterpret_cast<char*>(out.data()), bytes)) {
		out.clear();
		throw std::runtime_error("cannot read complete SOSD block");
	}
}
