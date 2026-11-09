#include "FileMappedData.h"
#include "JobMemoryLimit.h"
#include "QueryGenerator.h"
#include "Benchmark.h"
#include "BTree.h"
#include "FAST.h"

#include <Windows.h>
#include <Psapi.h>
#include <iostream>
#include <string>
#include <exception>

#pragma comment(lib, "Psapi.lib")
using namespace std;

void PrintMemoryUsage(const JobMemoryLimit& memoryLimit)
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};

    if (GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
        sizeof(pmc)))
    {
        double currentMB =
            pmc.PrivateUsage / (1024.0 * 1024.0);

        cout << "\n===== Memory Usage =====\n";
        cout << "Memory Limit: 512 MB\n";

        cout << "Current Private Usage: "
            << currentMB << " MB\n";

        size_t peakBytes = 0;

        if (memoryLimit.GetPeakProcessMemoryUsage(peakBytes))
        {
            double peakMB =
                peakBytes / (1024.0 * 1024.0);

            cout << "Peak Process Memory Usage: "
                << peakMB << " MB\n";
        }
        else
        {
            cout << "Peak Process Memory Usage: 확인 실패\n";
        }
    }
    else
    {
        cout << "메모리 사용량 확인 실패\n";
    }
}

static std::wstring ToWide(const char* s) {
    if (!s) {
        return L"";
    }

    int n = MultiByteToWideChar(
        CP_ACP,
        0,
        s,
        -1,
        nullptr,
        0
    );

    if (n <= 0) {
        return L"";
    }

   
    std::wstring result(n, L'\0');

    MultiByteToWideChar(
        CP_ACP,
        0,
        s,
        -1,
        result.data(),
        n
    );

    result.resize(n - 1);

    return result;
}

static std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) {
        return "";
    }

    int n = WideCharToMultiByte(
        CP_UTF8,
        0,
        s.c_str(),
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (n <= 0) {
        return "";
    }

    std::string result(n, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        s.c_str(),
        -1,
        result.data(),
        n,
        nullptr,
        nullptr
    );

    result.resize(n - 1);

    return result;
}

int main(int argc, char* argv[]) {

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::ios::sync_with_stdio(false);

    std::cout << "한글 출력 테스트\n";

    int choice;

    cout << "\n===== SOSD 검색 알고리즘 선택 =====\n";
    cout << "1. BTree\n";
    cout << "2. FAST\n";
    cout << "번호 입력: ";
    cin >> choice;

    if (choice != 1 && choice != 2) {
        cout << "잘못된 번호입니다.\n";
        return 1;
    }

    // 1. 메모리 제한 설정
    JobMemoryLimit memoryLimit;

    std::wstring error;

    constexpr size_t LimitBytes =
        512ull * 1024ull * 1024ull;

    if (!memoryLimit.Apply(LimitBytes, error)) {
        std::wcerr
            << L"메모리 제한 설정 실패: "
            << error
            << L'\n';

        return 1;
    }

    std::cout << "512MB memory limit applied.\n";

    // 2. 데이터 파일 경로 확인
    std::cout << "Argument count: " << argc << '\n';

    if (argc >= 2) {
        std::cout << "Dataset path: " << argv[1] << '\n';
    }

    if (argc < 2) {
        std::cout
            << "사용법: SOSD_Project.exe <dataset_path>\n";

        return 1;
    }

    // 3. File Mapping으로 데이터셋 열기
    std::cout << "Opening dataset...\n";

    FileMappedData dataset;

    if (!dataset.Open(ToWide(argv[1]), error)) {
        std::cerr
            << "데이터 열기 실패: "
            << ToUtf8(error)
            << '\n';

        return 1;
    }

    if (dataset.Size() > static_cast<uint64_t>(SIZE_MAX)) {
        std::cerr
            << "현재 플랫폼에서 처리할 수 없는 데이터 크기입니다.\n";

        return 1;
    }

    const uint32_t* data = dataset.Data();

    const size_t dataSize =
        static_cast<size_t>(dataset.Size());

    std::cout
        << "Dataset loaded through file mapping.\n";

    std::cout
        << "Key count: "
        << dataSize
        << '\n';

    std::cout
        << "File size: "
        << dataset.FileSize()
        << " bytes\n";

    // 4. 성공 및 실패 쿼리 생성
    constexpr size_t QueryCount = 10000;

    QuerySet queries;

    try {
        queries = QueryGenerator::Generate(
            data,
            dataSize,
            QueryCount
        );
    }
    catch (const std::exception& e) {
        std::cerr
            << "쿼리 생성 실패: "
            << e.what()
            << '\n';

        return 1;
    }

    std::cout
        << "Successful queries: "
        << queries.successful.size()
        << '\n';

    std::cout
        << "Unsuccessful queries: "
        << queries.unsuccessful.size()
        << '\n';

    // 5. BTree, FAST 측정
    if (choice == 1) {
        BTree btree;

        BenchmarkResult result =
            Benchmark::Run(btree, data, dataSize, queries);

        cout << "\n===== BTree 벤치마크 결과 =====\n";
        cout << "알고리즘: BTree\n";
        cout << "Build 시간: " << result.buildMs << " ms\n";
        cout << "성공 탐색 시간: " << result.successNsPerQuery << " ns/query\n";
        cout << "실패 탐색 시간: " << result.failureNsPerQuery << " ns/query\n";
        cout << "인덱스 메모리: " << result.indexBytes << " bytes\n";
        cout << "성공 쿼리 정확성: " << (result.successCorrect ? "통과" : "실패") << '\n';
        cout << "실패 쿼리 정확성: " << (result.failureCorrect ? "통과" : "실패") << '\n';

        Benchmark::AppendCsv("benchmark_results.csv", argv[1], result);
        PrintMemoryUsage(memoryLimit);
    }
    else if (choice == 2) {
        FAST fast;

        BenchmarkResult result =
            Benchmark::Run(fast, data, dataSize, queries);

        cout << "\n===== FAST 벤치마크 결과 =====\n";
        cout << "알고리즘: FAST\n";
        cout << "Build 시간: " << result.buildMs << " ms\n";
        cout << "성공 탐색 시간: " << result.successNsPerQuery << " ns/query\n";
        cout << "실패 탐색 시간: " << result.failureNsPerQuery << " ns/query\n";
        cout << "인덱스 메모리: " << result.indexBytes << " bytes\n";
        cout << "성공 쿼리 정확성: " << (result.successCorrect ? "통과" : "실패") << '\n';
        cout << "실패 쿼리 정확성: " << (result.failureCorrect ? "통과" : "실패") << '\n';

        Benchmark::AppendCsv("benchmark_results.csv", argv[1], result);
        PrintMemoryUsage(memoryLimit);
    }

    return 0;
 }