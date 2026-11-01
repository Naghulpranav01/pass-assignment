#include "http_server.hpp"
#include "fea_1d_solver.hpp"
#include "multicore_batch.hpp"
#include "backprop.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <shellapi.h>

void exportAllResults() {
    std::cout << "[Export] Generating Task C FEA convergence results...\n";
    auto feaStudy = Workbench::FEARodSolver::runConvergenceStudy();
    std::ofstream feaCsv("results/raw/fea_mesh_convergence.csv");
    feaCsv << feaStudy.toConvergenceCSV();
    feaCsv.close();

    // Export individual N=10, 20, 40, 80 profiles
    for (int N : {10, 20, 40, 80}) {
        Workbench::FEARodParams p;
        p.N = N;
        auto sol = Workbench::FEARodSolver::solve(p);
        std::ofstream pCsv("results/raw/fea_rod_N" + std::to_string(N) + ".csv");
        pCsv << sol.toCSV();
        pCsv.close();
    }

    std::cout << "[Export] Generating Task D Multicore benchmark results...\n";
    auto mcReport = Workbench::MulticoreBatchEngine::runBenchmark();
    std::ofstream mcCsv("results/raw/multicore_benchmark.csv");
    mcCsv << mcReport.toCSV();
    mcCsv.close();

    std::cout << "[Export] Generating Task F Backpropagation results...\n";
    auto bpReport = Workbench::BackpropEngine::runFullStudy();
    std::ofstream bpCsv("results/raw/backprop_100_steps.csv");
    bpCsv << bpReport.toCSV();
    bpCsv.close();

    std::cout << "[Export] All raw CSV files exported successfully to results/raw/!\n";
}

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n";
    std::cout << "       PASS Unified Engineering Workbench C++ Server    \n";
    std::cout << "       Target: Windows 11 x64 | C++17 Standard         \n";
    std::cout << "========================================================\n\n";

    bool exportOnly = false;
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--export" || arg == "--export-results") {
            exportOnly = true;
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
    }

    if (exportOnly) {
        exportAllResults();
        return 0;
    }

    // Determine frontend root path
    std::string frontendRoot = "src/frontend";
    // Check if running from root or build folder
    std::ifstream check("src/frontend/index.html");
    if (!check.is_open()) {
        std::ifstream checkParent("../src/frontend/index.html");
        if (checkParent.is_open()) {
            frontendRoot = "../src/frontend";
        }
    }

    Workbench::HttpServer server(port, frontendRoot);
    if (!server.start()) {
        std::cerr << "Failed to start HTTP server on port " << port << "\n";
        return 1;
    }

    std::string url = "http://127.0.0.1:" + std::to_string(server.getPort());
    std::cout << "[Server] Running at: " << url << "\n";
    std::cout << "[Server] Press Ctrl+C or Enter to shut down...\n";

    // Wait for user input or interruption
    std::cin.get();

    server.stop();
    return 0;
}
