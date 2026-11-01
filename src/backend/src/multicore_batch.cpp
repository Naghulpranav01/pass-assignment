#include "multicore_batch.hpp"
#include <random>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Workbench {

// BoundedThreadPool implementation
BoundedThreadPool::BoundedThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex_);
                    this->cvTaskAvailable_.wait(lock, [this]() {
                        return this->stop_ || !this->tasks_.empty();
                    });
                    if (this->stop_ && this->tasks_.empty()) {
                        return;
                    }
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                    this->activeTasks_++;
                }

                task();

                {
                    std::unique_lock<std::mutex> lock(this->queueMutex_);
                    this->activeTasks_--;
                    if (this->tasks_.empty() && this->activeTasks_ == 0) {
                        this->cvAllDone_.notify_all();
                    }
                }
            }
        });
    }
}

BoundedThreadPool::~BoundedThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    cvTaskAvailable_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void BoundedThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        tasks_.push(std::move(task));
    }
    cvTaskAvailable_.notify_one();
}

void BoundedThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    cvAllDone_.wait(lock, [this]() {
        return this->tasks_.empty() && this->activeTasks_ == 0;
    });
}

// Batch generator with fixed seed
std::vector<FEARodParams> MulticoreBatchEngine::generateBatch(const BatchRunConfig& config) {
    std::vector<FEARodParams> batch(config.caseCount);
    std::mt19937 rng(config.seed);

    std::uniform_real_distribution<double> distK(2.0, 50.0);       // W/(m K)
    std::uniform_real_distribution<double> distQ(2000.0, 40000.0);  // W/m^3
    std::uniform_real_distribution<double> distT0(50.0, 150.0);     // C
    std::uniform_real_distribution<double> distTL(10.0, 40.0);      // C

    for (int i = 0; i < config.caseCount; ++i) {
        FEARodParams p;
        p.L = 1.0;
        p.A = 1e-4;
        p.k = distK(rng);
        p.q = distQ(rng);
        p.T0 = distT0(rng);
        p.TL = distTL(rng);
        p.N = config.meshN;
        batch[i] = p;
    }
    return batch;
}

std::vector<SingleSolveOutput> MulticoreBatchEngine::solveSerial(const std::vector<FEARodParams>& batch) {
    std::vector<SingleSolveOutput> outputs(batch.size());
    for (size_t i = 0; i < batch.size(); ++i) {
        auto sol = FEARodSolver::solve(batch[i]);
        outputs[i].caseId = static_cast<int>(i);
        outputs[i].maxMidpointError = sol.maxMidpointError;
        int midNode = static_cast<int>(sol.nodes.size()) / 2;
        outputs[i].centerTemperature = sol.nodes[midNode].T_fea;
    }
    return outputs;
}

std::vector<SingleSolveOutput> MulticoreBatchEngine::solveParallel(
    const std::vector<FEARodParams>& batch, int numWorkers) {

    std::vector<SingleSolveOutput> outputs(batch.size());
    if (numWorkers <= 1) {
        return solveSerial(batch);
    }

    BoundedThreadPool pool(static_cast<size_t>(numWorkers));
    const size_t chunkSize = std::max(size_t{1}, batch.size() / (numWorkers * 8));

    for (size_t start = 0; start < batch.size(); start += chunkSize) {
        size_t end = std::min(batch.size(), start + chunkSize);
        pool.enqueue([&batch, &outputs, start, end]() {
            for (size_t i = start; i < end; ++i) {
                auto sol = FEARodSolver::solve(batch[i]);
                outputs[i].caseId = static_cast<int>(i);
                outputs[i].maxMidpointError = sol.maxMidpointError;
                int midNode = static_cast<int>(sol.nodes.size()) / 2;
                outputs[i].centerTemperature = sol.nodes[midNode].T_fea;
            }
        });
    }

    pool.waitAll();
    return outputs;
}

MulticoreBenchmarkReport MulticoreBatchEngine::runBenchmark(BatchRunConfig config) {
    MulticoreBenchmarkReport report;
    report.config = config;

    auto batch = generateBatch(config);

    // Warm-up run (single pass on 2 workers to prime CPU caches and JIT/OS scheduler)
    if (config.includeWarmup) {
        auto t0 = std::chrono::high_resolution_clock::now();
        solveParallel(batch, 2);
        auto t1 = std::chrono::high_resolution_clock::now();
        report.warmupTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    // Benchmark serial first to get baseline outputs for agreement checks
    std::vector<SingleSolveOutput> serialBaseline;

    for (int p : config.workerCounts) {
        BenchmarkWorkerResult res;
        res.workers = p;
        res.repetitionTimesMs.reserve(config.repetitions);

        std::vector<double> times;
        std::vector<SingleSolveOutput> lastOutputs;

        for (int r = 0; r < config.repetitions; ++r) {
            auto t0 = std::chrono::high_resolution_clock::now();
            if (p == 1) {
                lastOutputs = solveSerial(batch);
            } else {
                lastOutputs = solveParallel(batch, p);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            res.repetitionTimesMs.push_back(ms);
            times.push_back(ms);
        }

        if (p == 1) {
            serialBaseline = lastOutputs;
        }

        // Calculate median time
        std::sort(times.begin(), times.end());
        res.medianTimeMs = times[times.size() / 2];

        // Agreement check vs Serial Baseline
        double maxDiff = 0.0;
        if (!serialBaseline.empty() && !lastOutputs.empty()) {
            for (size_t i = 0; i < batch.size(); ++i) {
                double diff = std::abs(lastOutputs[i].centerTemperature - serialBaseline[i].centerTemperature);
                if (diff > maxDiff) maxDiff = diff;
            }
        }
        res.maxAbsoluteDifference = maxDiff;
        res.passesAgreement = (maxDiff <= 1e-8); // Stated requirement: <= 1e-8 C

        if (!res.passesAgreement) {
            report.overallAgreementVerified = false;
        }

        report.results.push_back(res);
    }

    // Compute speedup and efficiency relative to 1 worker median
    double t1_median = report.results.empty() ? 1.0 : report.results[0].medianTimeMs;
    for (auto& r : report.results) {
        r.speedup = t1_median / r.medianTimeMs;
        r.efficiency = r.speedup / r.workers;
    }

    report.agreementNote = report.overallAgreementVerified
        ? "Serial/parallel agreement verified: max absolute difference <= 1e-8 C across all cases."
        : "Agreement check failed: difference exceeded 1e-8 C.";

    return report;
}

std::string MulticoreBenchmarkReport::toCSV() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "# Multicore Benchmark Report\n";
    ss << "# CPU: " << cpuModel << ", Logical Cores: " << logicalCores
       << ", Physical Cores: " << physicalCores << ", RAM: " << totalRamGB << " GB\n";
    ss << "# Compiler: " << compiler << ", Flags: " << optimizationFlags << "\n";
    ss << "# Workload: " << config.caseCount << " independent solves, N=" << config.meshN
       << " elements per solve, Seed=" << config.seed << "\n";
    ss << "# Warmup Time: " << warmupTimeMs << " ms, Launch-to-Ready Time: " << launchToReadyTimeMs << " ms\n";
    ss << "workers,rep1_ms,rep2_ms,rep3_ms,rep4_ms,rep5_ms,median_ms,speedup_T1_Tp,efficiency,max_abs_diff_C,agreement_pass\n";

    for (const auto& r : results) {
        ss << r.workers << ",";
        for (double t : r.repetitionTimesMs) {
            ss << t << ",";
        }
        ss << r.medianTimeMs << "," << r.speedup << "," << r.efficiency << ","
           << r.maxAbsoluteDifference << "," << (r.passesAgreement ? "PASS" : "FAIL") << "\n";
    }
    return ss.str();
}

std::string MulticoreBenchmarkReport::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    ss << "  \"system\": {\n";
    ss << "    \"cpu\": \"" << cpuModel << "\",\n";
    ss << "    \"physicalCores\": " << physicalCores << ",\n";
    ss << "    \"logicalCores\": " << logicalCores << ",\n";
    ss << "    \"ramGB\": " << totalRamGB << ",\n";
    ss << "    \"compiler\": \"" << compiler << "\",\n";
    ss << "    \"flags\": \"" << optimizationFlags << "\"\n";
    ss << "  },\n";
    ss << "  \"workload\": {\n";
    ss << "    \"caseCount\": " << config.caseCount << ",\n";
    ss << "    \"meshN\": " << config.meshN << ",\n";
    ss << "    \"seed\": " << config.seed << ",\n";
    ss << "    \"warmupMs\": " << warmupTimeMs << ",\n";
    ss << "    \"launchToReadyMs\": " << launchToReadyTimeMs << "\n";
    ss << "  },\n";
    ss << "  \"overallAgreementVerified\": " << (overallAgreementVerified ? "true" : "false") << ",\n";
    ss << "  \"agreementNote\": \"" << agreementNote << "\",\n";
    ss << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        ss << "    {\n";
        ss << "      \"workers\": " << r.workers << ",\n";
        ss << "      \"repetitionsMs\": [";
        for (size_t j = 0; j < r.repetitionTimesMs.size(); ++j) {
            ss << r.repetitionTimesMs[j];
            if (j + 1 < r.repetitionTimesMs.size()) ss << ", ";
        }
        ss << "],\n";
        ss << "      \"medianMs\": " << r.medianTimeMs << ",\n";
        ss << "      \"speedup\": " << r.speedup << ",\n";
        ss << "      \"efficiency\": " << r.efficiency << ",\n";
        ss << "      \"maxAbsDiffC\": " << r.maxAbsoluteDifference << ",\n";
        ss << "      \"passesAgreement\": " << (r.passesAgreement ? "true" : "false") << "\n";
        ss << "    }";
        if (i + 1 < results.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace Workbench
