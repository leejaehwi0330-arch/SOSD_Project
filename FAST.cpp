#include "FAST.h"

#include <algorithm>
#include <climits>
#include <emmintrin.h> // SSE2: 128비트(32비트 정수 4개) SIMD
#include <utility>

bool FAST::Build(const uint32_t* data, size_t size) {
    if (size > 0 && data == nullptr) return false;

    data_ = data;
    size_ = size;
    levels_.clear();

    // 원본 정렬 배열을 256개씩 나누고 구간별 최댓값을 기록
    std::vector<uint32_t> childMax;
    for (size_t begin = 0; begin < size; begin += LeafCapacity) {
        size_t end = std::min(begin + LeafCapacity, size);
        childMax.push_back(data[end - 1]);
    }

    // 16개 구간을 하나의 64바이트 노드로 묶고 루트까지 반복
    while (childMax.size() > 1) {
        std::vector<Node> level;
        std::vector<uint32_t> parentMax;

        for (size_t first = 0; first < childMax.size(); first += Fanout) {
            size_t last = std::min(first + Fanout, childMax.size());

            Node node{};
            std::fill(std::begin(node.splitKeys), std::end(node.splitKeys), UINT32_MAX);

            // 마지막 자식의 최댓값은 저장하지 않아도 됨.
            // key가 앞의 15개 경계보다 크면 마지막 자식을 선택하면 됨.
            for (size_t i = first; i + 1 < last; ++i) {
                node.splitKeys[i - first] = childMax[i];
            }

            level.push_back(node);
            parentMax.push_back(childMax[last - 1]);
        }

        levels_.push_back(std::move(level));
        childMax = std::move(parentMax);
    }

    return true;
}

// SIMD로 분기 기준값 4개씩 비교하여 다음 자식 번호(0~15)를 찾음
size_t FAST::FindChild(const Node& node, uint32_t key) {
    // SSE2는 부호 있는 정수 비교를 지원하므로 최상위 비트를 뒤집어
    // uint32_t의 대소 관계를 그대로 비교하도록 만듦.
    const __m128i signBit = _mm_set1_epi32(INT32_MIN);
    const __m128i target = _mm_xor_si128(
        _mm_set1_epi32(static_cast<int>(key)), signBit);

    size_t child = 0;
    for (size_t i = 0; i < Fanout; i += 4) {
        // alignas(64) 때문에 16바이트 정렬된 메모리를 읽을 수 있음.
        const __m128i values = _mm_load_si128(
            reinterpret_cast<const __m128i*>(&node.splitKeys[i]));
        const __m128i orderedValues = _mm_xor_si128(values, signBit);
        const __m128i less = _mm_cmplt_epi32(orderedValues, target);
        const int mask = _mm_movemask_ps(_mm_castsi128_ps(less));

        if (mask == 15) {  // 네 값이 전부 key보다 작음
            child += 4;
            continue;
        }

        // mask의 1인 비트 개수 = key보다 작은 경계값 개수
        for (int bit = 0; bit < 4; ++bit) {
            child += (mask >> bit) & 1;
        }
        return child;
    }
    return child;
}

int64_t FAST::Search(uint32_t key) const {
    if (size_ == 0) return -1;

    size_t index = 0;

    // 루트부터 내려가며 검색할 리프 구간을 결정
    for (size_t l = levels_.size(); l > 0; --l) {
        const Node& node = levels_[l - 1][index];
        size_t child = FindChild(node, key);
        index = index * Fanout + child;
    }

    // 선택한 리프(최대 256개)에서 정확한 위치를 찾음
    const uint32_t* first = data_ + index * LeafCapacity;
    const uint32_t* last = data_ + std::min((index + 1) * LeafCapacity, size_);
    const uint32_t* found = std::lower_bound(first, last, key);

    if (found == last || *found != key) return -1;
    return static_cast<int64_t>(found - data_);
}

size_t FAST::GetMemoryUsage() const {
    size_t bytes = sizeof(*this) + levels_.capacity() * sizeof(std::vector<Node>);
    for (const auto& level : levels_) {
        bytes += level.capacity() * sizeof(Node);
    }
    return bytes; // 인덱스 메모리만 계산. 원본 data 배열은 별도.
}

std::string FAST::Name() const { return "FAST"; }
