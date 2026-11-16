#pragma once

#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

class TemporaryTestFiles {
public:
    TemporaryTestFiles() {
        const auto parent = std::filesystem::temp_directory_path();
        std::random_device random;
        for (int attempt = 0; attempt < 16; ++attempt) {
            directory_ = parent / ("sosd-test-" + std::to_string(random()));
            fixture_ = directory_ / "fixture.bin";
            if (std::filesystem::create_directory(directory_)) return;
        }
        throw std::runtime_error("cannot create temporary SOSD test directory");
    }

    TemporaryTestFiles(const TemporaryTestFiles&) = delete;
    TemporaryTestFiles& operator=(const TemporaryTestFiles&) = delete;

    ~TemporaryTestFiles() {
        std::error_code ignored;
        std::filesystem::remove(fixture_, ignored);
        std::filesystem::remove(directory_, ignored);
    }

    const std::filesystem::path& fixture() const { return fixture_; }
    std::filesystem::path missingFile() const { return directory_ / "no_such_file.bin"; }

private:
    std::filesystem::path directory_;
    std::filesystem::path fixture_;
};
