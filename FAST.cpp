
#include "FAST.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <emmintrin.h>
#include <limits>
#include <new>
#include <utility>

namespace {
    constexpr size_t MemoryLimit = 512ull * 1024 * 1024;

    inline size_t FindSimdChild(
        const uint32_t* threeKeys,
        __m128i query)
    {
        const __m128i sign = _mm_set1_epi32(INT32_MIN);

        const __m128i raw = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(threeKeys));

        const __m128i keys = _mm_xor_si128(raw, sign);
        const __m128i comparison = _mm_cmpgt_epi32(query, keys);

        const int bits = _mm_movemask_ps(
            _mm_castsi128_ps(comparison));

        const size_t right = static_cast<size_t>(bits & 1);
        const size_t child = static_cast<size_t>(
            (bits >> (right ? 2 : 1)) & 1);

        return right * 2 + child;
    }

    inline size_t FindCacheChild(
        const uint32_t* cacheLine,
        __m128i query)
    {
        const size_t top = FindSimdChild(cacheLine, query);

        const size_t bottom = FindSimdChild(
            cacheLine + 3 + top * 3,
            query);

        return top * 4 + bottom;
    }
}

uint32_t FAST::Pivot(size_t index) const
{
    return index < size_ ? data_[index] : UINT32_MAX;
}

void FAST::WriteSimdBlock(
    uint32_t* dest,
    size_t begin,
    size_t span) const
{
    const size_t quarter = span / 4;

    dest[0] = Pivot(begin + 2 * quarter - 1);
    dest[1] = Pivot(begin + quarter - 1);
    dest[2] = Pivot(begin + 3 * quarter - 1);
}

void FAST::WriteCacheLine(
    uint32_t* dest,
    size_t begin,
    size_t span) const
{
    WriteSimdBlock(dest, begin, span);

    const size_t quarter = span / 4;

    for (size_t child = 0; child < 4; ++child) {
        WriteSimdBlock(
            dest + 3 + child * 3,
            begin + child * quarter,
            quarter);
    }

    dest[15] = UINT32_MAX;
}

void FAST::WritePageBlock(
    uint32_t* dest,
    size_t begin,
    size_t span) const
{
    WriteCacheLine(dest, begin, span);

    const size_t part = span / 16;

    for (size_t child = 0; child < 16; ++child) {
        WriteCacheLine(
            dest + (child + 1) * 16,
            begin + child * part,
            part);
    }
}

uint32_t* FAST::BlockAt(
    const PageLevel& level,
    size_t blockIndex)
{
    const size_t offset =
        (blockIndex / BlocksPerPage) * PageBytes
        + (blockIndex % BlocksPerPage) * BlockBytes;

    return level.aligned + offset / sizeof(uint32_t);
}

bool FAST::Build(
    const uint32_t* data,
    size_t size)
{
    pageLevels_.clear();
    data_ = nullptr;
    size_ = 0;
    logicalLeaves_ = 0;

    if (size > 0 && data == nullptr) {
        return false;
    }

    // 원본 데이터는 제외하고 FAST 인덱스 메모리만 계산한다.
    size_t memoryNeeded = sizeof(*this);

    size_t leaves = 1;

    while (leaves <= size) {
        if (leaves >
            std::numeric_limits<size_t>::max() / 2) {
            return false;
        }

        leaves *= 2;
    }

    data_ = data;
    size_ = size;
    if (size > 0) { minKey_ = data[0]; maxKey_ = data[size - 1]; }
    logicalLeaves_ = leaves;

    try {
        for (size_t span = leaves;
            span >= (size_t(1) << PageDepth);
            span /= (size_t(1) << PageDepth)) {

            const size_t count =
                size / span + (size % span != 0);

            const size_t pages =
                count / BlocksPerPage
                + (count % BlocksPerPage != 0);

            if (pages >
                (std::numeric_limits<size_t>::max() - PageBytes)
                / PageBytes) {
                throw std::bad_alloc();
            }

            const size_t payload = pages * PageBytes;
            const size_t allocated = payload + PageBytes;

            // FAST 인덱스의 추가 메모리만 512MB 기준으로 검사한다.
            if (memoryNeeded > MemoryLimit ||
                allocated > MemoryLimit - memoryNeeded ||
                sizeof(PageLevel) >
                MemoryLimit - memoryNeeded - allocated) {

                pageLevels_.clear();
                data_ = nullptr;
                size_ = 0;
                logicalLeaves_ = 0;

                return false;
            }

            memoryNeeded += allocated + sizeof(PageLevel);

            PageLevel level;

            level.blockCount = count;
            level.leafSpan = span;
            level.storageBytes = allocated;

            level.storage.reset(
                new uint32_t[allocated / sizeof(uint32_t)]);

            const uintptr_t raw =
                reinterpret_cast<uintptr_t>(level.storage.get());

            const uintptr_t aligned =
                (raw + PageBytes - 1)
                & ~uintptr_t(PageBytes - 1);

            level.aligned = reinterpret_cast<uint32_t*>(aligned);

            for (size_t block = 0; block < count; ++block) {
                WritePageBlock(
                    BlockAt(level, block),
                    block * span,
                    span);
            }

            pageLevels_.push_back(std::move(level));
        }
    }
    catch (const std::bad_alloc&) {
        pageLevels_.clear();
        data_ = nullptr;
        size_ = 0;
        logicalLeaves_ = 0;

        return false;
    }

    return true;
}

int64_t FAST::Search(uint32_t key) const
{
    if (size_ == 0 || key < minKey_ || key > maxKey_) return -1;

    const __m128i query = _mm_set1_epi32(
        static_cast<int>(key ^ 0x80000000u));

    size_t prefix = 0;
    size_t span = logicalLeaves_;

    for (const PageLevel& level : pageLevels_) {
        if (prefix >= level.blockCount) {
            return -1;
        }

        const uint32_t* block = BlockAt(level, prefix);

        const size_t first = FindCacheChild(block, query);

        const size_t second = FindCacheChild(
            block + 16 * (first + 1),
            query);

        prefix = prefix * 256 + first * 16 + second;
        span /= 256;
    }

    const size_t start = prefix * span;

    if (start >= size_) {
        return -1;
    }

    const size_t end = start + std::min(span, size_ - start);

    const uint32_t* found = std::lower_bound(
        data_ + start,
        data_ + end,
        key);

    if (found == data_ + end || *found != key) {
        return -1;
    }

    return static_cast<int64_t>(found - data_);
}

size_t FAST::GetMemoryUsage() const
{
    size_t bytes =
        sizeof(*this)
        + pageLevels_.capacity() * sizeof(PageLevel);

    for (const PageLevel& level : pageLevels_) {
        bytes += level.storageBytes;
    }

    return bytes;
}

std::string FAST::Name() const
{
    return "FAST";
}
