#pragma once
#define NOMINMAX
#include <Windows.h>
#include <string>
#include <cstddef>

class JobMemoryLimit {
public:
    ~JobMemoryLimit();

    bool Apply(
        size_t limitBytes,
        std::wstring& error
    );

private:
    HANDLE job_ = nullptr;
};
