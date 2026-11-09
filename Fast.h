#pragma once

#include "SearchIndex.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// FAST의 기본 아이디어를 적용한 간단한 검색 인덱스
// - 64바이트 캐시 라인에 분기 기준값 16개를 모음
// - SSE2 SIMD 명령어로 4개씩 비교
// - 원본 정렬 배열에서는 이진 탐색
class FAST final : public SearchIndex {
public:
    bool Build(const uint32_t* data, size_t size) override;
    int64_t Search(uint32_t key) const override;
    size_t GetMemoryUsage() const override;
    std::string Name() const override;

private:
    static constexpr size_t LeafCapacity = 256;
    static constexpr size_t Fanout = 16;

    // uint32_t 16개 = 64바이트 (대부분의 CPU에서 캐시 라인 1개)
    // 실제 경계값은 최대 15개이고, 나머지 칸은 UINT32_MAX로 채움.
    struct alignas(64) Node {
        uint32_t splitKeys[Fanout];
    };

    static size_t FindChild(const Node& node, uint32_t key);

    // levels_[0]은 리프 바로 위의 노드들, levels_.back()[0]은 루트
    std::vector<std::vector<Node>> levels_;
    const uint32_t* data_ = nullptr;
    size_t size_ = 0;
};

