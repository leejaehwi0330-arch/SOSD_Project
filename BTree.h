#pragma once

#include "SearchIndex.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class BTree final : public SearchIndex {
public:
    BTree() = default;
    ~BTree() override = default;

    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;

    // SearchIndex
    bool Build(const uint32_t* data, size_t size) override;
    int64_t Search(uint32_t key) const override;
    size_t GetMemoryUsage() const override;
    std::string Name() const override;

private:
    static constexpr size_t LeafCapacity = 256;  // 리프 하나당 원소 수
    static constexpr size_t Fanout = 8;          // 내부 노드 하나당 자식 수

    struct Node {
        std::vector<uint32_t> maxKeys;   // 자식 서브트리의 최댓값
        std::vector<uint32_t> children;  // 자식의 번호
    };

    std::vector<std::vector<Node>> levels;  // levels[0]의 자식은 리프, 루트는 levels.back()[0]

    const uint32_t* data_ = nullptr;
    size_t size_ = 0;
};
