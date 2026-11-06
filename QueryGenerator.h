#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

struct QuerySet {
    std::vector<uint32_t> successful;
    std::vector<uint32_t> unsuccessful;
};

class QueryGenerator {
public:
    static QuerySet Generate(
        const uint32_t* data,
        size_t size,
        size_t queryCount,
        uint32_t seed = 20261105
    );

private:
    static bool Contains(
        const uint32_t* data,
        size_t size,
        uint32_t key
    );
};
