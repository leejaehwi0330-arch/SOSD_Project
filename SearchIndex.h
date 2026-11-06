#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

class SearchIndex {
public:
    virtual ~SearchIndex() = default;

    virtual bool Build(const uint32_t* data, size_t size) = 0;

    virtual int64_t Search(uint32_t key) const = 0;

    virtual size_t GetMemoryUsage() const = 0;

    virtual std::string Name() const = 0;
};
