#pragma once

#include "SearchIndex.h"
#include "QueryGenerator.h"

#include <cstddef>
#include <cstdint>
#include <string>

struct BenchmarkResult {
    std::string algorithm;

    double buildMs = 0.0;

    double successNsPerQuery = 0.0;

    double failureNsPerQuery = 0.0;

    size_t indexBytes = 0;

    bool successCorrect = false;

    bool failureCorrect = false;
};

class Benchmark {
public:
    static BenchmarkResult Run(
        SearchIndex& index,
        const uint32_t* data,
        size_t dataSize,
        const QuerySet& queries
    );

    static bool AppendCsv(
        const std::string& path,
        const std::string& dataset,
        const BenchmarkResult& result
    );
};