#include "JobMemoryLimit.h"

JobMemoryLimit::~JobMemoryLimit() {
    if (job_) {
        CloseHandle(job_);
    }
}

bool JobMemoryLimit::Apply(
    size_t limitBytes,
    std::wstring& error
) {
    job_ = CreateJobObjectW(
        nullptr,
        nullptr
    );

    if (!job_) {
        error = L"CreateJobObject 실패";
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};

    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_PROCESS_MEMORY;

    info.ProcessMemoryLimit =
        static_cast<SIZE_T>(limitBytes);

    if (!SetInformationJobObject(
        job_,
        JobObjectExtendedLimitInformation,
        &info,
        sizeof(info)
    )) {
        error = L"SetInformationJobObject 실패";
        return false;
    }

    if (!AssignProcessToJobObject(
        job_,
        GetCurrentProcess()
    )) {
        error = L"AssignProcessToJobObject 실패";
        return false;
    }

    return true;
}

bool JobMemoryLimit::GetPeakProcessMemoryUsage(size_t& bytes) const
{
    if (!job_) {
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};

    if (!QueryInformationJobObject(
        job_,
        JobObjectExtendedLimitInformation,
        &info,
        sizeof(info),
        nullptr))
    {
        return false;
    }

    bytes = static_cast<size_t>(info.PeakProcessMemoryUsed);

    return true;
}