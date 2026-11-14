#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>

#include "SOSDReader.h"
#include "BinarySearch.h"
#include "BlockIndex.h"

#pragma comment(lib, "psapi.lib")

static double nowMs() { 
	using namespace std::chrono; 
	return duration_cast<microseconds>(
		steady_clock::now().time_since_epoch()).count() / 1000.0; 
}

static size_t peakMemoryKB() { 
	PROCESS_MEMORY_COUNTERS pmc; 
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) 
		return pmc.PeakWorkingSetSize / 1024; 
	return 0; 
}

int main(int argc, char** argv) { 
	if (argc < 3) { 
		std::cerr << "Usage: SOSD.exe <datafile_uint32> <blockSize>\n"; 
		return 1; 
	} 
	std::string path = argv[1]; 
	uint32_t blockSize = static_cast<uint32_t>(std::stoul(argv[2])); 
	
	std::cout << "file: " << path << "\n"; 
	SOSDReader reader(path); 
	std::cout << "n = " << reader.size() << "\n"; 
	
	const int NUM_QUERIES = 1000; 
	std::mt19937 rng(42); 
	std::uniform_int_distribution<uint64_t> dist(0, reader.size() - 1); 
	std::vector<uint32_t> hitQueries, missQueries; 
	
	for (int i = 0; i < NUM_QUERIES; ++i) 
		hitQueries.push_back(static_cast<uint32_t>(reader.readKey(dist(rng)))); 
	
	std::uniform_int_distribution<uint32_t> missDist(1, UINT32_MAX); 
	for (int i = 0; i < NUM_QUERIES; ++i) 
		missQueries.push_back(missDist(rng)); 
	
	BinarySearch bs(reader); 
	double t0 = nowMs(); 
	bs.build(); 
	double buildBs = nowMs() - t0; 
	double hitBs = 0, missBs = 0; 
	t0 = nowMs(); 
	for (auto k : hitQueries) bs.find(k); 
	hitBs = nowMs() - t0; 
	t0 = nowMs(); 
	for (auto k : missQueries) bs.find(k); 
	missBs = nowMs() - t0; 
	size_t memBs = peakMemoryKB(); 
	
	BlockIndex bi(reader, blockSize); 
	t0 = nowMs(); 
	bi.build(); 
	double buildBi = nowMs() - t0; 
	double hitBi = 0, missBi = 0; 
	t0 = nowMs(); 
	for (auto k : hitQueries) bi.find(k); 
	hitBi = nowMs() - t0; t0 = nowMs(); 
	for (auto k : missQueries) bi.find(k); 
	missBi = nowMs() - t0; 
	size_t memBi = peakMemoryKB(); 
	
	std::cout << "\n[Full binary search]\n" 
		<< "build_ms=" << buildBs << "\n" 
		<< "hit_1000_ms=" << hitBs << "\n" 
		<< "miss_1000_ms=" << missBs << "\n" 
		<< "total_ms=" << buildBs + hitBs + missBs << "\n" 
		<< "peak_mem_kb=" << memBs << "\n"; 
	
	std::cout << "\n[Block index, blockSize=" << blockSize << "]\n" 
		<< "build_ms=" << buildBi << "\n" 
		<< "hit_1000_ms=" << hitBi << "\n" 
		<< "miss_1000_ms=" << missBi << "\n" 
		<< "total_ms=" << buildBi + hitBi + missBi << "\n" 
		<< "peak_mem_kb=" << memBi << "\n"; 
	
	return 0; 
}
