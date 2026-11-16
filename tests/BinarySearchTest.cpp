// 기준 탐색(BinarySearch) 검증 프로그램 - 담당 2
// 빌드(Linux/Mac): g++ -std=c++17 -O2 -I.. BinarySearchTest.cpp ../BinarySearch.cpp ../SOSDReader.cpp -o bs_test
// 빌드(Windows): VS 개발자 명령 프롬프트에서 tests 폴더로 이동한 뒤 실행
//   cl /utf-8 /std:c++17 /EHsc /O2 /I.. BinarySearchTest.cpp ..\BinarySearch.cpp ..\SOSDReader.cpp /Fe:bs_test.exe
// main.cpp(측정용)와는 별개 프로그램이므로 SOSD 메인 프로젝트에는 추가하지 않는다.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include "BinarySearch.h"
#include "TemporaryTestFiles.h"

// SOSD 형식: [uint64 원소 개수][uint32 키 * n]
static void writeSosd(const std::filesystem::path& path, const std::vector<uint32_t>& keys) {
    std::ofstream f(path, std::ios::binary);
    uint64_t n = keys.size();
    f.write(reinterpret_cast<const char*>(&n), 8);
    if (n) f.write(reinterpret_cast<const char*>(keys.data()), n * 4);
    f.close();
    if (!f) throw std::runtime_error("cannot write binary search test fixture");
}

static int failed = 0, passed = 0;

static void check(const std::string& label, uint64_t got, uint64_t expected) {
    if (got == expected) { ++passed; return; }
    ++failed;
    auto s = [](uint64_t v) { return v == NOT_FOUND ? std::string("NOT_FOUND") : std::to_string(v); };
    std::cout << "FAIL " << label << ": got " << s(got) << ", expected " << s(expected) << "\n";
}

// 정답 계산: 정렬된 vector에서 첫 일치 위치
static uint64_t oracle(const std::vector<uint32_t>& v, uint32_t key) {
    auto it = std::lower_bound(v.begin(), v.end(), key);
    return (it != v.end() && *it == key) ? uint64_t(it - v.begin()) : NOT_FOUND;
}

int main() try {
    // ---- 1) 소형 검증 파일 + 정답표 ----
    struct Case { const char* what; uint32_t key; uint64_t expected; };
    const Case cases[] = {
        {"첫 키",            5,    0},
        {"마지막 키",        60,   9},
        {"중복 키(10)",      10,   1},
        {"중복 키(30)",      30,   5},
        {"중간 존재 키",     40,   7},
        {"중간에 없는 키",   15,   NOT_FOUND},
        {"중간에 없는 키2",  45,   NOT_FOUND},
        {"범위 밖(작음)",    1,    NOT_FOUND},
        {"범위 밖(큼)",      100,  NOT_FOUND},
        {"키 0",             0,    NOT_FOUND},
        {"키 UINT32_MAX",    UINT32_MAX, NOT_FOUND},
    };
    {
        SOSDReader r("small_test.bin");
        BinarySearch bs(r);
        bs.build();
        std::cout << "[소형 파일 정답표] n=" << r.size() << "\n";
        for (auto& c : cases) {
            uint64_t got = bs.find(c.key);
            std::cout << "  " << c.what << " key=" << c.key << " -> "
                      << (got == NOT_FOUND ? std::string("NOT_FOUND") : std::to_string(got))
                      << (got == c.expected ? "  OK" : "  FAIL") << "\n";
            check(c.what, got, c.expected);
        }
    }

    const TemporaryTestFiles temp;
    const auto fixture = temp.fixture().string();

    // ---- 2) 경계 크기 파일: n=0, n=1, n=2 ----
    for (std::vector<uint32_t> v : {std::vector<uint32_t>{}, std::vector<uint32_t>{7},
                                    std::vector<uint32_t>{7, 7}}) {
        writeSosd(temp.fixture(), v);
        SOSDReader r(fixture);
        BinarySearch bs(r);
        for (uint32_t k : {0u, 6u, 7u, 8u, UINT32_MAX})
            check("n=" + std::to_string(v.size()) + " key=" + std::to_string(k), bs.find(k), oracle(v, k));
    }

    // ---- 3) 중복이 매우 많은 파일: 전체가 같은 키 / 긴 중복 구간 ----
    {
        std::vector<uint32_t> v(5000, 42);
        v.insert(v.end(), 3000, 100);
        writeSosd(temp.fixture(), v);
        SOSDReader r(fixture);
        BinarySearch bs(r);
        check("dup 42 첫 위치", bs.find(42), 0);
        check("dup 100 첫 위치", bs.find(100), 5000);
        check("dup 사이 없는 키", bs.find(70), NOT_FOUND);
    }

    // ---- 4) 무작위 대량 검증: 정답(std::lower_bound)과 전수 비교 ----
    {
        std::mt19937 rng(123);
        std::vector<uint32_t> v(20000);
        for (auto& x : v) x = rng() % 50000;          // 중복 다수
        std::sort(v.begin(), v.end());
        writeSosd(temp.fixture(), v);
        SOSDReader r(fixture);
        BinarySearch bs(r);
        for (uint32_t k = 0; k < 50010; ++k)           // 존재/부재 키 전부
            check("rand key=" + std::to_string(k), bs.find(k), oracle(v, k));
        for (int i = 0; i < 5000; ++i) {                // 임의 32비트 키
            uint32_t k = rng();
            check("rand32 key=" + std::to_string(k), bs.find(k), oracle(v, k));
        }
    }

    std::cout << "\n통과 " << passed << " / 실패 " << failed << "\n";
    return failed ? 1 : 0;
} catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n';
    return 2;
}
