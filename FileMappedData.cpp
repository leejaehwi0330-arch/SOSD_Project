#include "FileMappedData.h"

#include <limits>
#include <cstring>

FileMappedData::~FileMappedData() {
    Close();
}

void FileMappedData::Close() {
    data_ = nullptr;
    count_ = 0;
    fileSize_ = 0;

    if (view_) {
        UnmapViewOfFile(view_);
        view_ = nullptr;
    }

    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }

    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
}

bool FileMappedData::Open(
    const std::wstring& path,
    std::wstring& error
) {
    Close();

    file_ = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file_ == INVALID_HANDLE_VALUE) {
        error = L"데이터 파일을 열 수 없습니다.";
        return false;
    }

    LARGE_INTEGER size{};

    if (!GetFileSizeEx(file_, &size) || size.QuadPart < 0) {
        error = L"파일 크기를 확인할 수 없습니다.";
        Close();
        return false;
    }

    fileSize_ = static_cast<uint64_t>(size.QuadPart);

    if (fileSize_ < HeaderBytes) {
        error = L"파일 크기가 헤더보다 작습니다.";
        Close();
        return false;
    }

    mapping_ = CreateFileMappingW(
        file_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );

    if (!mapping_) {
        error = L"CreateFileMapping 실패";
        Close();
        return false;
    }

    view_ = MapViewOfFile(
        mapping_,
        FILE_MAP_READ,
        0,
        0,
        0
    );

    if (!view_) {
        error = L"MapViewOfFile 실패";
        Close();
        return false;
    }

    uint64_t headerCount = 0;

    const auto* bytes =
        static_cast<const unsigned char*>(view_);

    memcpy(
        &headerCount,
        bytes,
        sizeof(headerCount)
    );

    if (headerCount >
        (std::numeric_limits<uint64_t>::max() - HeaderBytes)
        / sizeof(uint32_t)) {

        error = L"데이터 개수가 잘못되었습니다.";
        Close();
        return false;
    }

    const uint64_t expected =
        HeaderBytes + headerCount * sizeof(uint32_t);

    if (expected != fileSize_) {
        error = L"데이터 파일 형식이 올바르지 않습니다.";
        Close();
        return false;
    }

    count_ = headerCount;

    data_ = reinterpret_cast<const uint32_t*>(
        bytes + HeaderBytes
        );

    return true;
}