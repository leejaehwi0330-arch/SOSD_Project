#include "QueryGenerator.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <limits>

bool QueryGenerator::Contains(
    const uint32_t* data,
    size_t size,
    uint32_t key
) {
    return std::binary_search(
        data,
        data + size,
        key
    );
}

QuerySet QueryGenerator::Generate(
    const uint32_t* data,
    size_t size,
    size_t queryCount,
    uint32_t seed
) {
    if (!data || size == 0) {
        throw std::runtime_error(
            "데이터가 비어 있습니다."
        );
    }

    QuerySet q;

    q.successful.reserve(queryCount);
    q.unsuccessful.reserve(queryCount);

    std::mt19937 rng(seed);

    std::uniform_int_distribution<size_t> indexDist(
        0,
        size - 1
    );

    std::uniform_int_distribution<uint32_t> keyDist(
        0,
        std::numeric_limits<uint32_t>::max()
    );

    // 성공 쿼리 생성
    for (size_t i = 0; i < queryCount; ++i) {
        q.successful.push_back(
            data[indexDist(rng)]
        );
    }

    // 실패 쿼리 생성
    while (q.unsuccessful.size() < queryCount) {
        uint32_t key = keyDist(rng);

        if (!Contains(data, size, key)) {
            q.unsuccessful.push_back(key);
        }
    }

    return q;
}