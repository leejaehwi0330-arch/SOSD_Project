#include "BTree.h"

#include <algorithm>
#include <utility>

bool BTree::Build(const uint32_t* data, size_t size) {

    data_ = data;
    size_ = size;
    if (size > 0) { minKey_ = data[0]; maxKey_ = data[size - 1]; }

    // 원본 배열을 LeafCapacity개씩 끊어서 리프 노드를 만든다
    // 각 리프의 최댓값 childMax[i] = data[(i + 1) * LeafCapacity - 1]
    std::vector<uint32_t> childMax;
    for (size_t begin = 0; begin < size; begin += LeafCapacity) {
        const size_t end = std::min(begin + LeafCapacity, size);
        childMax.push_back(data[end - 1]);
    }

    // 루트까지 리프를 Fanout개씩 묶은 노드 레벨을 만들어 올라간다
    while (childMax.size() > 1) {
        std::vector<Node> level;        // 노드를 저장할 레벨
        std::vector<uint32_t> nodeMax;  // 각 노드의 최댓값

        // childMax를 Fanout개씩 묶어서 노드를 만든다
        for (size_t first = 0; first < childMax.size(); first += Fanout) {
            const size_t last = std::min(first + Fanout, childMax.size());

            Node node;
            node.maxKeys.assign(childMax.begin() + first, childMax.begin() + last);
            node.children.resize(last - first);
            for (size_t i = 0; i < node.children.size(); i++) {
                node.children[i] = static_cast<uint32_t>(first + i);
            }

            nodeMax.push_back(node.maxKeys.back());
            level.push_back(std::move(node));   // 만들어진 노드를 현 레벨에 추가
        }
        levels.push_back(std::move(level));     // 현 레벨을 트리에 추가

        // 다음 레벨을 만들기 위해 childMax를 nodeMax로 갱신
        childMax = std::move(nodeMax);
    }

    return true;
}

int64_t BTree::Search(uint32_t key) const {
    if (size_ == 0 || key < minKey_ || key > maxKey_) return -1;
    size_t index = 0;

    // 루트에서 시작해서, 최댓값이 key 이상인 첫 번째 자식으로 탐색
    for (size_t l = levels.size(); l > 0; l--) {
        const Node& node = levels[l - 1][index];

        // LeafCapacity 256, Fanout 64 기준 탐색 결과

        // 이진 탐색 430ns, 390ns
        /*
        const auto iter = std::lower_bound(node.maxKeys.begin(), node.maxKeys.end(), key);
        if (iter == node.maxKeys.end()) return -1;          // key가 이 서브트리의 모든 값보다 크다
        index = node.children[iter - node.maxKeys.begin()]; // 다음 레벨로 내려갈 자식 번호
        */

        // 선형 탐색 670ns, 680ns
        size_t childIndex = 0;
        while (childIndex < node.maxKeys.size() && node.maxKeys[childIndex] < key)
            childIndex++;   // 검색 키 이상의 첫 번째 최댓값을 탐색
        if (childIndex == node.maxKeys.size()) return -1;
        index = node.children[childIndex];
    }

    // 리프 구간에서 이진 탐색
    const uint32_t* first = data_ + index * LeafCapacity;
    const uint32_t* last = data_ + std::min((index + 1) * LeafCapacity, size_);
    const uint32_t* found = std::lower_bound(first, last, key);

    if (found == last || *found != key) return -1;  // key가 없음

    return found - data_;
}

size_t BTree::GetMemoryUsage() const {
    size_t bytes = sizeof(*this) + levels.capacity() * sizeof(std::vector<Node>);

    for (const std::vector<Node>& level : levels) {
        bytes += level.capacity() * sizeof(Node);
        for (const Node& node : level) {
            bytes += node.maxKeys.capacity() * sizeof(uint32_t);
            bytes += node.children.capacity() * sizeof(uint32_t);
        }
    }
    return bytes;
}

std::string BTree::Name() const { return "BTree"; }

