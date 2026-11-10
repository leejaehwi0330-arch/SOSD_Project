
#pragma once

#include "SearchIndex.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class FAST final : public SearchIndex {
public:
    bool Build(const uint32_t* data, size_t size) override;
    int64_t Search(uint32_t key) const override;
    size_t GetMemoryUsage() const override;
    std::string Name() const override;

private:
    static constexpr size_t PageBytes = 4096;
    static constexpr size_t CacheLineBytes = 64;
    static constexpr size_t PageDepth = 8;
    static constexpr size_t CacheDepth = 4;
    static constexpr size_t SimdDepth = 2;

    static constexpr size_t CacheLinesPerBlock = 17;
    static constexpr size_t BlockBytes =
        CacheLinesPerBlock * CacheLineBytes;
    static constexpr size_t BlocksPerPage = 3;

    struct PageLevel {
        size_t blockCount = 0;
        size_t leafSpan = 0;
        size_t storageBytes = 0;

        std::unique_ptr<uint32_t[]> storage;
        uint32_t* aligned = nullptr;
    };

    uint32_t Pivot(size_t index) const;

    void WriteSimdBlock(
        uint32_t* dest,
        size_t begin,
        size_t span) const;

    void WriteCacheLine(
        uint32_t* dest,
        size_t begin,
        size_t span) const;

    void WritePageBlock(
        uint32_t* dest,
        size_t begin,
        size_t span) const;

    static uint32_t* BlockAt(
        const PageLevel& level,
        size_t blockIndex);

    std::vector<PageLevel> pageLevels_;

    const uint32_t* data_ = nullptr;
    size_t size_ = 0;
    size_t logicalLeaves_ = 0;
};
