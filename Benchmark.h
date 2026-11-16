#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "BinarySearch.h"

struct BenchmarkQuery {
	uint32_t key;
	uint64_t expected;
};

struct BenchmarkQueries {
	std::vector<BenchmarkQuery> hits;
	std::vector<BenchmarkQuery> misses;
	std::size_t inRangeMisses = 0;
};

namespace benchmark_detail {

inline uint32_t findGuaranteedGap(SOSDReader& reader, uint32_t firstKey) {
	uint64_t left = 0;
	uint64_t right = reader.size() - 1;
	uint64_t leftKey = firstKey;
	// Keep an interval whose key span exceeds the number of index steps.
	while (right - left > 1) {
		const uint64_t middle = left + (right - left) / 2;
		const uint64_t middleKey = reader.readKey(middle);
		if (middleKey - leftKey > middle - left) {
			right = middle;
		} else {
			left = middle;
			leftKey = middleKey;
		}
	}
	return static_cast<uint32_t>(leftKey + 1);
}

using Clock = std::chrono::steady_clock;

inline double elapsedMs(Clock::time_point start) {
	return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

inline double measureQueries(Searcher& searcher, const std::vector<BenchmarkQuery>& queries) {
	std::vector<uint64_t> results(queries.size());
	const auto start = Clock::now();
	for (std::size_t i = 0; i < queries.size(); ++i) {
		results[i] = searcher.find(queries[i].key);
	}
	const double elapsed = elapsedMs(start);
	for (std::size_t i = 0; i < queries.size(); ++i) {
		if (results[i] != queries[i].expected) {
			throw std::runtime_error(std::string(searcher.name()) +
				" returned an unexpected result for key " + std::to_string(queries[i].key));
		}
	}
	return elapsed;
}

} // namespace benchmark_detail

inline BenchmarkQueries makeBenchmarkQueries(SOSDReader& reader, std::size_t queriesPerKind) {
	const uint64_t count = reader.size();
	if (count == 0) {
		throw std::invalid_argument("cannot benchmark an empty SOSD dataset");
	}
	if (queriesPerKind == 0) {
		throw std::invalid_argument("benchmark query count must be positive");
	}

	BenchmarkQueries queries;
	queries.hits.reserve(queriesPerKind);
	queries.misses.reserve(queriesPerKind);
	BinarySearch oracle(reader);
	std::mt19937 rng(42);
	std::uniform_int_distribution<uint64_t> samplePosition(0, count - 1);
	for (std::size_t i = 0; i < queriesPerKind; ++i) {
		const auto key = static_cast<uint32_t>(reader.readKey(samplePosition(rng)));
		const uint64_t position = oracle.find(key);
		if (position == NOT_FOUND) {
			throw std::runtime_error("sampled key not found in SOSD dataset");
		}
		queries.hits.push_back({key, position});
	}

	const auto first = static_cast<uint32_t>(reader.readKey(0));
	const auto last = static_cast<uint32_t>(reader.readKey(count - 1));
	if (first > last) {
		throw std::runtime_error("SOSD dataset must be sorted");
	}
	if (first < last) {
		std::uniform_int_distribution<uint32_t> sampleKey(first, last);
		for (int pass = 0; pass < 4 && queries.misses.size() < queriesPerKind; ++pass) {
			for (std::size_t i = 0; i < queriesPerKind && queries.misses.size() < queriesPerKind; ++i) {
				const uint32_t key = sampleKey(rng);
				if (oracle.find(key) == NOT_FOUND) {
					queries.misses.push_back({key, NOT_FOUND});
				}
			}
		}
	}
	if (queries.misses.empty() && uint64_t{last} - first > count - 1) {
		const uint32_t key = benchmark_detail::findGuaranteedGap(reader, first);
		if (oracle.find(key) != NOT_FOUND) {
			throw std::runtime_error("failed to verify an in-range missing key");
		}
		queries.misses.push_back({key, NOT_FOUND});
	}
	queries.inRangeMisses = queries.misses.size();

	const uint64_t outsideCount = uint64_t{first} + (uint64_t{UINT32_MAX} - last);
	if (outsideCount != 0) {
		std::uniform_int_distribution<uint64_t> sampleOutside(0, outsideCount - 1);
		while (queries.misses.size() < queriesPerKind) {
			const uint64_t offset = sampleOutside(rng);
			const uint64_t key = offset < first ? offset : uint64_t{last} + 1 + (offset - first);
			queries.misses.push_back({static_cast<uint32_t>(key), NOT_FOUND});
		}
	} else if (queries.misses.empty()) {
		throw std::runtime_error("no missing key found within the benchmark sampling limit");
	} else {
		std::uniform_int_distribution<std::size_t> sampleVerified(0, queries.inRangeMisses - 1);
		while (queries.misses.size() < queriesPerKind) {
			const BenchmarkQuery sample = queries.misses[sampleVerified(rng)];
			queries.misses.push_back(sample);
		}
		queries.inRangeMisses = queries.misses.size();
	}
	return queries;
}

inline void runBenchmark(Searcher& searcher, const BenchmarkQueries& queries) {
	const auto start = benchmark_detail::Clock::now();
	searcher.build();
	const double buildMs = benchmark_detail::elapsedMs(start);
	const double hitMs = benchmark_detail::measureQueries(searcher, queries.hits);
	const double missMs = benchmark_detail::measureQueries(searcher, queries.misses);
	std::cout << '\n' << '[' << searcher.name() << "]\n"
		<< "build_ms=" << buildMs << '\n'
		<< "hit_queries=" << queries.hits.size() << '\n'
		<< "hit_ms=" << hitMs << '\n'
		<< "miss_queries=" << queries.misses.size() << '\n'
		<< "in_range_miss_queries=" << queries.inRangeMisses << '\n'
		<< "miss_ms=" << missMs << '\n'
		<< "total_ms=" << buildMs + hitMs + missMs << '\n';
}
