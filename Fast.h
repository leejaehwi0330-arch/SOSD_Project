#pragma once

#include "SearchIndex.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 계층적 블로킹을 적용한 FAST 방식의 이진 탐색 인덱스.
// 이진 트리 → 8단계 페이지 블록 → 4단계 캐시라인 블록
//                         → 2단계 SSE2 SIMD 블록.
// 입력 데이터는 오름차순 정렬되어 있어야 하며 Build() 실행 후에도 유지되어야 한다.
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

    // 이진 트리의 8단계 블록은 상위 4단계 캐시라인 블록 1개와
    // 하위 4단계 캐시라인 블록 16개로 구성된다(17 × 64 = 1088바이트).
    static constexpr size_t CacheLinesPerBlock = 17;
    static constexpr size_t BlockBytes = CacheLinesPerBlock * CacheLineBytes;
    static constexpr size_t BlocksPerPage = 3; // 3 × 1088 = 3264바이트, 한 페이지(4096바이트) 안에 들어감

    struct PageLevel {
        size_t blockCount = 0;
        size_t leafSpan = 0;
        size_t storageBytes = 0;
        std::unique_ptr<uint32_t[]> storage;
        uint32_t* aligned = nullptr;
    };

    uint32_t Pivot(size_t index) const;
    void WriteSimdBlock(uint32_t* dest, size_t begin, size_t leafSpan) const;
    void WriteCacheLine(uint32_t* dest, size_t begin, size_t leafSpan) const;
    void WritePageBlock(uint32_t* dest, size_t begin, size_t leafSpan) const;
    static uint32_t* BlockAt(const PageLevel& level, size_t blockIndex);

    std::vector<PageLevel> pageLevels_;
    const uint32_t* data_ = nullptr;
    size_t size_ = 0;
    size_t logicalLeaves_ = 0;
};
