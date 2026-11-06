#pragma once
#define NOMINMAX
#include <Windows.h>
#include <cstdint>
#include <string>

class FileMappedData {
public:
    FileMappedData() = default;
    ~FileMappedData();

    FileMappedData(const FileMappedData&) = delete;
    FileMappedData& operator=(const FileMappedData&) = delete;

    bool Open(const std::wstring& path, std::wstring& error);

    void Close();

    const uint32_t* Data() const {
        return data_;
    }

    uint64_t Size() const {
        return count_;
    }

    uint64_t FileSize() const {
        return fileSize_;
    }

private:
    HANDLE file_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_ = nullptr;
    void* view_ = nullptr;

    const uint32_t* data_ = nullptr;

    uint64_t count_ = 0;
    uint64_t fileSize_ = 0;

    static constexpr uint64_t HeaderBytes = sizeof(uint64_t);
};
