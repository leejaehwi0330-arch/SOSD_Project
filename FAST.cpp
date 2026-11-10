#include "FAST.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <emmintrin.h>  // SSE2는 64비트 Windows에서도 지원됨
#include <limits>
#include <new>

namespace {
    constexpr size_t MemoryLimit = 512ull * 1024 * 1024;

    // SIMD 블록 하나는 이진 탐색 트리의 2단계를 나타낸다.
    // [루트, 왼쪽 자식, 오른쪽 자식] 순서로 저장하며 네 번째 칸은 무시한다.
    // 가능한 4개 하위 경로 중 하나를 반환한다: 0=왼쪽→왼쪽, 1=왼쪽→오른쪽, 2=오른쪽→왼쪽, 3=오른쪽→오른쪽.
    inline size_t FindSimdChild(const uint32_t* threeKeys, __m128i query) {
        const __m128i sign = _mm_set1_epi32(INT32_MIN);
        const __m128i raw = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(threeKeys));
        const __m128i keys = _mm_xor_si128(raw, sign);
        const __m128i comparison = _mm_cmpgt_epi32(query, keys);
        const int bits = _mm_movemask_ps(_mm_castsi128_ps(comparison));

        const size_t right = static_cast<size_t>(bits & 1);
        const size_t child = static_cast<size_t>((bits >> (right ? 2 : 1)) & 1);
        return right * 2 + child;
    }

    // 64바이트 캐시라인 하나에 이진 탐색 트리의 4단계를 저장한다(기준값 15개 + 빈칸 1개).
    // 기준값이 3개인 SIMD 작은 트리 5개를 합쳐 구성한다.
    inline size_t FindCacheChild(const uint32_t* cacheLine, __m128i query) {
        const size_t top = FindSimdChild(cacheLine, query);
        const size_t bottom = FindSimdChild(cacheLine + 3 + top * 3, query);
        return top * 4 + bottom; // 결과 범위: 0~15
    }
}

uint32_t FAST::Pivot(size_t index) const {
    // 논리적인 완전 이진 트리에서 실제 데이터가 없는 오른쪽 끝만 채움값으로 처리한다.
    return index < size_ ? data_[index] : UINT32_MAX;
}

void FAST::WriteSimdBlock(uint32_t* dest, size_t begin, size_t span) const {
    // 이진 탐색의 기준값은 후보 위치를 절반씩 나눈다.
    const size_t quarter = span / 4;
    dest[0] = Pivot(begin + 2 * quarter - 1); // 루트
    dest[1] = Pivot(begin + quarter - 1);      // 왼쪽 자식
    dest[2] = Pivot(begin + 3 * quarter - 1);  // 오른쪽 자식
}

void FAST::WriteCacheLine(uint32_t* dest, size_t begin, size_t span) const {
    // 이진 트리 4단계 = 상위 2단계 SIMD 블록 1개 + 하위 SIMD 블록 4개.
    WriteSimdBlock(dest, begin, span);
    const size_t quarter = span / 4;
    for (size_t child = 0; child < 4; ++child) {
        WriteSimdBlock(dest + 3 + child * 3, begin + child * quarter, quarter);
    }
    dest[15] = UINT32_MAX; // 마지막 SSE 데이터 읽기에 필요한 네 번째 칸
}

void FAST::WritePageBlock(uint32_t* dest, size_t begin, size_t span) const {
    // 처음 4단계는 캐시라인 1개에 저장한다.
    WriteCacheLine(dest, begin, span);

    // 다음 4단계는 캐시라인 16개에 저장하며, 처음 캐시라인의 결과로 하나를 선택한다.
    const size_t part = span / 16;
    for (size_t child = 0; child < 16; ++child) {
        WriteCacheLine(dest + (child + 1) * 16,
                       begin + child * part, part);
    }
}

uint32_t* FAST::BlockAt(const PageLevel& level, size_t blockIndex) {
    // 1088바이트짜리 트리 블록 3개를 4096바이트 페이지 하나에 배치한다.
    const size_t offset = (blockIndex / BlocksPerPage) * PageBytes
                        + (blockIndex % BlocksPerPage) * BlockBytes;
    return level.aligned + offset / sizeof(uint32_t);
}

bool FAST::Build(const uint32_t* data, size_t size) {
    pageLevels_.clear();
    data_ = nullptr;
    size_ = 0;
    logicalLeaves_ = 0;

    if (size > 0 && data == nullptr) return false;
    if (size > MemoryLimit / sizeof(uint32_t)) return false;

    // 데이터 사이의 삽입 위치 개수를 2의 거듭제곱으로 올림한다.
    // 빈 영역을 채우는 기준값은 UINT32_MAX이며 오른쪽 끝에만 존재한다.
    size_t leaves = 1;
    while (leaves <= size) {
        if (leaves > std::numeric_limits<size_t>::max() / 2) return false;
        leaves *= 2;
    }

    data_ = data;
    size_ = size;
    logicalLeaves_ = leaves;

    size_t memoryNeeded = size * sizeof(uint32_t); // 외부에서 전달된 원본 데이터 배열
    try {
        for (size_t span = leaves; span >= (size_t(1) << PageDepth);
             span /= (size_t(1) << PageDepth)) {
            const size_t count = size / span + (size % span != 0);
            const size_t pages = count / BlocksPerPage
                                 + (count % BlocksPerPage != 0);
            if (pages > (std::numeric_limits<size_t>::max() - (PageBytes - 1))
                        / PageBytes) {
                throw std::bad_alloc();
            }
            const size_t payload = pages * PageBytes;
            const size_t allocated = payload + PageBytes;

            // Build()에서 원본 배열과 FAST 인덱스를 합친 크기가 프로젝트 메모리 제한을 넘지 않게 한다.
            if (allocated > MemoryLimit - memoryNeeded) {
                pageLevels_.clear();
                data_ = nullptr;
                size_ = 0;
                logicalLeaves_ = 0;
                return false;
            }
            memoryNeeded += allocated;

            PageLevel level;
            level.blockCount = count;
            level.leafSpan = span;
            level.storageBytes = allocated;
            level.storage.reset(new uint32_t[allocated / sizeof(uint32_t)]);
            const uintptr_t raw = reinterpret_cast<uintptr_t>(level.storage.get());
            const uintptr_t aligned = (raw + PageBytes - 1)
                                       & ~uintptr_t(PageBytes - 1);
            level.aligned = reinterpret_cast<uint32_t*>(aligned);

            for (size_t block = 0; block < count; ++block) {
                WritePageBlock(BlockAt(level, block), block * span, span);
            }
            pageLevels_.push_back(std::move(level));
        }
    } catch (const std::bad_alloc&) {
        pageLevels_.clear();
        data_ = nullptr;
        size_ = 0;
        logicalLeaves_ = 0;
        return false;
    }
    return true;
}

int64_t FAST::Search(uint32_t key) const {
    if (size_ == 0) return -1;

    // 부호 없는 정수 비교를 부호 있는 정수용 SSE2 비교로 변환한다.
    // 최상위 부호 비트를 뒤집으면 0~UINT32_MAX 값의 대소 관계가 유지된다.
    const __m128i query = _mm_set1_epi32(static_cast<int>(key ^ 0x80000000u));
    size_t prefix = 0;
    size_t span = logicalLeaves_;

    for (const PageLevel& level : pageLevels_) {
        if (prefix >= level.blockCount) return -1;
        const uint32_t* block = BlockAt(level, prefix);

        // 캐시라인 2개를 차례로 읽어 논리적인 이진 트리의 8단계를 탐색한다.
        const size_t first = FindCacheChild(block, query);
        const size_t second = FindCacheChild(block + 16 * (first + 1), query);
        prefix = prefix * 256 + first * 16 + second;
        span /= 256;
    }

    // 마지막에 남은 불완전한 하위 트리(최대 7단계)는
    // 수백 개 노드의 채움값을 만들지 않고 원본 정렬 배열에서 바로 탐색한다.
    const size_t start = prefix * span;
    if (start >= size_) return -1;
    const size_t end = start + std::min(span, size_ - start);
    const uint32_t* found = std::lower_bound(data_ + start, data_ + end, key);
    if (found == data_ + end || *found != key) return -1;
    return static_cast<int64_t>(found - data_);
}

size_t FAST::GetMemoryUsage() const {
    size_t bytes = sizeof(*this)
                 + pageLevels_.capacity() * sizeof(PageLevel);
    for (const PageLevel& level : pageLevels_) {
        bytes += level.storageBytes;
    }
    return bytes; // FAST 인덱스만 계산하며 원본 데이터 배열은 제외한다.
}

std::string FAST::Name() const { return "FAST"; }
