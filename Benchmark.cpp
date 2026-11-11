#include "Benchmark.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
using namespace std;

BenchmarkResult Benchmark::Run(
    SearchIndex& index,
    const uint32_t* data,
    size_t dataSize,
    const QuerySet& queries
) {
    BenchmarkResult r;

    r.algorithm = index.Name();
    {
        volatile uint32_t sink = 0;
        uint32_t acc = 0;
        for (size_t i = 0; i < dataSize; i += 1024) acc += data[i];  // 4KB마다 1번
        sink = acc;
    }

    // Build 시간 측정
    auto buildStart =
        std::chrono::steady_clock::now();

    bool built = index.Build(
        data,
        dataSize
    );

    auto buildEnd =
        std::chrono::steady_clock::now();

    r.buildMs =
        std::chrono::duration<double, std::milli>(
            buildEnd - buildStart
        ).count();

    if (!built) {
        return r;
    }

    r.indexBytes = index.GetMemoryUsage();

    // 성공 쿼리 정확성 검사
    r.successCorrect = true;

    for (size_t i = 0; i < queries.successful.size(); ++i) {
        uint32_t key = queries.successful[i];

        int64_t pos = index.Search(key);

        if (
            pos < 0 ||
            static_cast<uint64_t>(pos) >= dataSize ||
            data[pos] != key
            ) {
            r.successCorrect = false;

            int64_t expected =
                std::lower_bound(data, data + dataSize, key) - data;

            cout << "\n[Success Query Error]\n";
            cout << "Query Index: " << i << '\n';
            cout << "Key: " << key << '\n';
            cout << "Expected: " << expected << '\n';
            cout << "Actual: " << pos << '\n';

            break;
        }
    }

    // 실패 쿼리 정확성 검사
    r.failureCorrect = true;

    for (size_t i = 0; i < queries.unsuccessful.size(); ++i) {
        uint32_t key = queries.unsuccessful[i];

        int64_t actual = index.Search(key);

        if (actual != -1) {
            r.failureCorrect = false;

            cout << "\n[Failure Query Error]\n";
            cout << "Query Index: " << i << '\n';
            cout << "Key: " << key << '\n';
            cout << "Expected: -1\n";
            cout << "Actual: " << actual << '\n';

            break;
        }
    }

    // 실제 성공 탐색 시간 측정
    volatile int64_t checksum = 0;

    auto s1 =
        std::chrono::steady_clock::now();

    for (uint32_t key : queries.successful) {
        checksum = checksum + index.Search(key);
    }

    auto s2 =
        std::chrono::steady_clock::now();

    // 실제 실패 탐색 시간 측정
    auto f1 =
        std::chrono::steady_clock::now();

    for (uint32_t key : queries.unsuccessful) {
        checksum = checksum + index.Search(key);
    }

    auto f2 =
        std::chrono::steady_clock::now();

    (void)checksum;

    const size_t successN =
        queries.successful.size();

    const size_t failureN =
        queries.unsuccessful.size();

    if (successN > 0) {
        r.successNsPerQuery =
            std::chrono::duration<double, std::nano>(
                s2 - s1
            ).count() / successN;
    }

    if (failureN > 0) {
        r.failureNsPerQuery =
            std::chrono::duration<double, std::nano>(
                f2 - f1
            ).count() / failureN;
    }

    return r;
}

bool Benchmark::AppendCsv(
    const std::string& path,
    const std::string& dataset,
    const BenchmarkResult& r
) {
    std::ifstream check(path);

    bool exists = check.good();

    check.close();

    std::ofstream out(
        path,
        std::ios::app
    );

    if (!out) {
        return false;
    }

    if (!exists) {
        out
            << "Dataset,Algorithm,Build_ms,"
            << "Success_ns_per_query,"
            << "Failure_ns_per_query,"
            << "Index_Memory_bytes,"
            << "Success_Correct,"
            << "Failure_Correct\n";
    }

    out
        << dataset << ','
        << r.algorithm << ','
        << std::fixed
        << std::setprecision(3)
        << r.buildMs << ','
        << r.successNsPerQuery << ','
        << r.failureNsPerQuery << ','
        << r.indexBytes << ','
        << (r.successCorrect ? "TRUE" : "FALSE") << ','
        << (r.failureCorrect ? "TRUE" : "FALSE")
        << '\n';

    return true;
}
