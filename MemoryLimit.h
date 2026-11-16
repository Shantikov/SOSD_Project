#pragma once

#include <cstdint>
#include <memory>
#include <system_error>
#include <windows.h>

class ProcessMemoryLimit {
public:
    static constexpr std::uint64_t limitBytes = 512'000'000;

    ProcessMemoryLimit() : job_(::CreateJobObjectW(nullptr, nullptr)) {
        if (!job_) {
            throwLastError("CreateJobObjectW for the process memory limit");
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit = static_cast<SIZE_T>(limitBytes);
        if (!::SetInformationJobObject(job_.get(), JobObjectExtendedLimitInformation,
                                       &limits, static_cast<DWORD>(sizeof(limits)))) {
            throwLastError("SetInformationJobObject for the 512 MB process memory limit");
        }
        if (!::AssignProcessToJobObject(job_.get(), ::GetCurrentProcess())) {
            throwLastError("AssignProcessToJobObject for the 512 MB process memory limit");
        }
    }

    ProcessMemoryLimit(const ProcessMemoryLimit&) = delete;
    ProcessMemoryLimit& operator=(const ProcessMemoryLimit&) = delete;

    std::uint64_t peakBytes() const {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        if (!::QueryInformationJobObject(job_.get(), JobObjectExtendedLimitInformation,
                                         &limits, static_cast<DWORD>(sizeof(limits)), nullptr)) {
            throwLastError("QueryInformationJobObject for peak committed process memory");
        }
        return static_cast<std::uint64_t>(limits.PeakProcessMemoryUsed);
    }

private:
    struct CloseJobHandle {
        void operator()(HANDLE job) const noexcept {
            ::CloseHandle(job);
        }
    };

    [[noreturn]] static void throwLastError(const char* operation) {
        const auto error = static_cast<int>(::GetLastError());
        throw std::system_error(error, std::system_category(), operation);
    }

    std::unique_ptr<void, CloseJobHandle> job_;
};
