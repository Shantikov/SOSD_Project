// 공용 판독 모듈(SOSDReader) 검증 프로그램 - 담당 1
// 빌드(g++): tests 폴더에서
//   g++ -std=c++17 -O2 -I.. SOSDReaderTest.cpp ../SOSDReader.cpp -o reader_test
// 빌드(Windows): VS 개발자 명령 프롬프트에서 tests 폴더로 이동한 뒤 실행
//   cl /utf-8 /std:c++17 /EHsc /O2 /I.. SOSDReaderTest.cpp ..\SOSDReader.cpp /Fe:reader_test.exe
// small_test.bin, small_test_expected.txt 는 현재 폴더, tests/, ../tests/ 순서로 찾는다
// (tests 폴더, 프로젝트 루트, Visual Studio 프로젝트 폴더 어디서 실행해도 된다).
// 실제 SOSD 파일의 헤더 확인: reader_test <datafile_uint32>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "SOSDReader.h"

static int failed = 0, passed = 0;

static void check(const std::string& label, bool ok) {
    if (ok) { ++passed; return; }
    ++failed;
    std::cout << "FAIL " << label << "\n";
}

template <class E>
static void expectThrow(const std::string& label, const std::function<void()>& f) {
    try { f(); } catch (const E&) { ++passed; return; } catch (...) {}
    ++failed;
    std::cout << "FAIL " << label << " (expected exception)\n";
}

static void writeRaw(const std::string& path, uint64_t n, const void* data, size_t bytes) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&n), 8);
    if (bytes) f.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
}

// 검증용 데이터 파일을 실행 위치와 무관하게 찾는다.
static std::string dataPath(const std::string& name) {
    for (const char* dir : {"", "tests/", "../tests/"}) {
        const std::string p = dir + name;
        if (std::ifstream(p, std::ios::binary)) return p;
    }
    throw std::runtime_error("cannot find test data file '" + name +
                             "' (looked in ./, tests/, ../tests/)");
}

static void printHeader(const SOSDReader& r) {
    std::cout << "file=" << r.path() << "\n"
              << "  file_bytes=" << r.fileSize() << "\n"
              << "  header_bytes=" << r.headerSize() << "\n"
              << "  key_bytes=" << r.keySize() << "\n"
              << "  n=" << r.size() << "\n";
    if (r.size() > 0) {
        std::cout << "  first_key=" << r.readKey(0)
                  << "  last_key=" << r.readKey(r.size() - 1) << "\n";
    }
}

static int run(int argc, char** argv) {
    // 인자로 실제 SOSD 파일을 주면 헤더 정보만 출력한다.
    if (argc > 1) {
        try {
            SOSDReader r(argv[1]);
            printHeader(r);
        } catch (const std::exception& e) {
            std::cout << "ERROR: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    const std::vector<uint32_t> keys = {5, 10, 10, 10, 20, 30, 30, 40, 50, 60};

    // ---- 1) 헤더 / 키 크기 / 원소 개수 ----
    {
        SOSDReader r(dataPath("small_test.bin"));
        printHeader(r);
        check("n == 10", r.size() == 10);
        check("keySize == 4", r.keySize() == 4);
        check("headerSize == 8", r.headerSize() == 8);
        check("fileSize == 48", r.fileSize() == 48);

        // ---- 2) 위치 기반 단일 키 읽기 (순방향, 역방향) ----
        for (uint64_t i = 0; i < keys.size(); ++i)
            check("readKey(" + std::to_string(i) + ")", r.readKey(i) == keys[i]);
        for (uint64_t i = keys.size(); i-- > 0;)
            check("readKey reverse(" + std::to_string(i) + ")", r.readKey(i) == keys[i]);
        expectThrow<std::out_of_range>("readKey(n)", [&] { r.readKey(10); });
        expectThrow<std::out_of_range>("readKey(UINT64_MAX)", [&] { r.readKey(UINT64_MAX); });
        check("readKey after out_of_range", r.readKey(3) == 10);

        // ---- 3) 연속 블록 읽기: 모든 (start, count) 조합 ----
        std::vector<uint32_t> out;
        for (uint64_t start = 0; start <= 12; ++start) {
            for (uint32_t count = 0; count <= 12; ++count) {
                out.assign(3, 999);  // 이전 내용이 남지 않는지 확인
                r.readBlock(start, count, out);
                std::vector<uint32_t> want;
                for (uint64_t i = start; i < start + count && i < keys.size(); ++i)
                    want.push_back(keys[i]);
                check("readBlock(" + std::to_string(start) + "," + std::to_string(count) + ")",
                      out == want);
            }
        }
        r.readBlock(0, UINT32_MAX, out);
        check("readBlock(0, UINT32_MAX) clipped", out == keys);
        r.readBlock(9, UINT32_MAX, out);
        check("readBlock(9, UINT32_MAX) clipped", out == std::vector<uint32_t>{60});
        check("readKey after readBlock", r.readKey(0) == 5);

        // ---- 4) 정답표(small_test_expected.txt)가 파일 내용과 일치하는지 ----
        std::ifstream exp(dataPath("small_test_expected.txt"));
        check("open small_test_expected.txt", static_cast<bool>(exp));
        std::string line;
        int rows = 0;
        while (std::getline(exp, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            uint64_t key = 0;
            long long expected = 0;
            if (!(ss >> key >> expected)) { check("parse '" + line + "'", false); continue; }
            long long actual = -1;  // 선형 탐색으로 첫 위치를 구한다
            for (uint64_t i = 0; i < r.size(); ++i)
                if (r.readKey(i) == key) { actual = static_cast<long long>(i); break; }
            check("expected table key=" + std::to_string(key), actual == expected);
            ++rows;
        }
        check("expected table has rows", rows > 0);
        std::cout << "expected table rows checked: " << rows << "\n";
    }

    // ---- 5) 빈 파일(n=0) ----
    {
        writeRaw("reader_tmp.bin", 0, nullptr, 0);
        SOSDReader r("reader_tmp.bin");
        check("empty n == 0", r.size() == 0);
        expectThrow<std::out_of_range>("empty readKey(0)", [&] { r.readKey(0); });
        std::vector<uint32_t> out = {1, 2};
        r.readBlock(0, 4, out);
        check("empty readBlock", out.empty());
    }

    // ---- 6) 잘못된 파일은 생성자에서 거부 ----
    expectThrow<std::runtime_error>("missing file", [] { SOSDReader r("no_such_file.bin"); });
    {
        std::ofstream("reader_tmp.bin", std::ios::binary).write("abc", 3);
        expectThrow<std::runtime_error>("short header", [] { SOSDReader r("reader_tmp.bin"); });

        writeRaw("reader_tmp.bin", 10, keys.data(), 9 * 4);      // 키 1개 부족
        expectThrow<std::runtime_error>("truncated data", [] { SOSDReader r("reader_tmp.bin"); });

        writeRaw("reader_tmp.bin", 5, keys.data(), 10 * 4);      // 헤더보다 데이터가 많음
        expectThrow<std::runtime_error>("extra data", [] { SOSDReader r("reader_tmp.bin"); });

        const std::vector<uint64_t> wide = {1, 2, 3};            // uint64 데이터셋
        writeRaw("reader_tmp.bin", 3, wide.data(), 3 * 8);
        expectThrow<std::runtime_error>("uint64 dataset", [] { SOSDReader r("reader_tmp.bin"); });

        writeRaw("reader_tmp.bin", UINT64_MAX, keys.data(), 4);  // n*4 오버플로
        expectThrow<std::runtime_error>("huge n", [] { SOSDReader r("reader_tmp.bin"); });
    }
    std::remove("reader_tmp.bin");

    std::cout << "\npassed " << passed << " / failed " << failed << "\n";
    return failed ? 1 : 0;
}

int main(int argc, char** argv) {
    // 예외가 main 밖으로 나가면 abort() 대화상자만 뜨므로 여기서 원인을 출력한다.
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << "\n";
        return 2;
    }
}
