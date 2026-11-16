#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "Benchmark.h"
#include "BinarySearch.h"
#include "BlockIndex.h"
#include "MemoryLimit.h"
#include "SOSDReader.h"

namespace {

constexpr std::uint32_t defaultBlockSize = 4096;
constexpr std::uint32_t defaultQueriesPerKind = 1000;

std::uint32_t parseNumber(std::string_view text, const char* label, bool positive = false) {
    std::uint32_t value = 0;
    const auto last = text.data() + text.size();
    const auto result = std::from_chars(text.data(), last, value);
    if (result.ec != std::errc{} || result.ptr != last || (positive && value == 0)) {
        throw std::invalid_argument(std::string(label) +
            (positive ? " must be a positive 32-bit integer" : " must be an unsigned 32-bit integer"));
    }
    return value;
}

void findKey(const char* path, std::uint32_t key, std::uint32_t blockSize) {
    SOSDReader reader(path);
    BlockIndex searcher(reader, blockSize);
    searcher.build();
    const auto position = searcher.find(key);
    if (position == NOT_FOUND) {
        std::cout << "NOT_FOUND\n";
    } else {
        std::cout << position << '\n';
    }
}

void benchmark(const char* path, std::string_view method, std::uint32_t blockSize,
               std::uint32_t queriesPerKind, const ProcessMemoryLimit& memoryLimit) {
    SOSDReader reader(path);
    const auto queries = makeBenchmarkQueries(reader, queriesPerKind);
    if (method == "binary" || method == "both") {
        BinarySearch searcher(reader);
        runBenchmark(searcher, queries);
    }
    if (method == "block" || method == "both") {
        BlockIndex searcher(reader, blockSize);
        std::cout << "block_size=" << blockSize << '\n';
        runBenchmark(searcher, queries);
    }
    std::cout << "peak_process_commit_bytes=" << memoryLimit.peakBytes() << '\n';
}

void printUsage(std::ostream& out) {
    out << "Usage:\n"
        << "  SOSD.exe find <datafile_uint32> <key> [blockSize]\n"
        << "  SOSD.exe bench <datafile_uint32> binary [queriesPerKind]\n"
        << "  SOSD.exe bench <datafile_uint32> block <blockSize> [queriesPerKind]\n"
        << "  SOSD.exe <datafile_uint32> <blockSize>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        printUsage(std::cout);
        return argc == 1 ? 1 : 0;
    }

    try {
        const ProcessMemoryLimit memoryLimit;
        const std::string_view mode = argv[1];
        if (mode == "find" && (argc == 4 || argc == 5)) {
            const auto key = parseNumber(argv[3], "key");
            const auto blockSize = argc == 5 ? parseNumber(argv[4], "block size", true)
                                             : defaultBlockSize;
            findKey(argv[2], key, blockSize);
            return 0;
        }
        if (mode == "bench" && argc >= 4) {
            const std::string_view method = argv[3];
            if (method == "binary" && (argc == 4 || argc == 5)) {
                const auto count = argc == 5 ? parseNumber(argv[4], "queries per kind", true)
                                             : defaultQueriesPerKind;
                benchmark(argv[2], method, defaultBlockSize, count, memoryLimit);
                return 0;
            }
            if (method == "block" && (argc == 5 || argc == 6)) {
                const auto blockSize = parseNumber(argv[4], "block size", true);
                const auto count = argc == 6 ? parseNumber(argv[5], "queries per kind", true)
                                             : defaultQueriesPerKind;
                benchmark(argv[2], method, blockSize, count, memoryLimit);
                return 0;
            }
        }
        if (mode != "find" && mode != "bench" && argc == 3) {
            benchmark(argv[1], "both", parseNumber(argv[2], "block size", true),
                      defaultQueriesPerKind, memoryLimit);
            return 0;
        }
        printUsage(std::cerr);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
