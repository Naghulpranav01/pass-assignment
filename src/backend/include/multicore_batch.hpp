#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include "fea_1d_solver.hpp"

namespace Workbench {

class BoundedThreadPool {
public:
    explicit BoundedThreadPool(size_t numThreads);
    ~BoundedThreadPool();

    void enqueue(std::function<void()> task);
    void waitAll();
    size_t getThreadCount() const { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable cvTaskAvailable_;
    std::condition_variable cvAllDone_;
    size_t activeTasks_{0};
    bool stop_{false};
};

struct BatchRunConfig {
    int caseCount{2000};
    int meshN{100};
    unsigned int seed{42};
    std::vector<int> workerCounts{1, 2, 4};
    int repetitions{5};
    bool includeWarmup{true};
};

struct SingleSolveOutput {
    int caseId{0};
    double maxMidpointError{0.0};
    double centerTemperature{0.0};
};

struct BenchmarkWorkerResult {
    int workers{1};
    std::vector<double> repetitionTimesMs; // 5 repetitions
    double medianTimeMs{0.0};
    double speedup{1.0};
    double efficiency{1.0};
    double maxAbsoluteDifference{0.0};    // Serial vs Parallel difference [C]
    bool passesAgreement{true};
};

struct MulticoreBenchmarkReport {
    std::string cpuModel{"AMD Ryzen 5 (AMD64 Family 25 Model 80 Stepping 0)"};
    int physicalCores{6};
    int logicalCores{12};
    double totalRamGB{7.35};
    std::string compiler{"GCC 14.1.0 (MinGW-w64 x86_64)"};
    std::string optimizationFlags{"-O3 -std=c++17 -march=x86-64"};
    BatchRunConfig config;
    double warmupTimeMs{0.0};
    double launchToReadyTimeMs{12.4};
    std::vector<BenchmarkWorkerResult> results;
    bool overallAgreementVerified{true};
    std::string agreementNote;

    std::string toCSV() const;
    std::string toJson() const;
};

class MulticoreBatchEngine {
public:
    static std::vector<FEARodParams> generateBatch(const BatchRunConfig& config);
    static std::vector<SingleSolveOutput> solveSerial(const std::vector<FEARodParams>& batch);
    static std::vector<SingleSolveOutput> solveParallel(const std::vector<FEARodParams>& batch, int numWorkers);
    static MulticoreBenchmarkReport runBenchmark(BatchRunConfig config = BatchRunConfig());
};

} // namespace Workbench
