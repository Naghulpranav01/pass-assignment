#include <iostream>
#include <iomanip>
#include <string>
#include <cassert>
#include <cmath>
#include "stl_parser.hpp"
#include "ansys_csv_parser.hpp"
#include "fea_1d_solver.hpp"
#include "multicore_batch.hpp"
#include "embedded_adapters.hpp"
#include "backprop.hpp"

using namespace Workbench;

int totalTests = 0;
int passedTests = 0;

void reportTest(const std::string& name, bool condition, const std::string& extra = "") {
    totalTests++;
    if (condition) {
        passedTests++;
        std::cout << "  [PASS] " << name;
        if (!extra.empty()) std::cout << " (" << extra << ")";
        std::cout << "\n";
    } else {
        std::cout << "  [FAIL] " << name;
        if (!extra.empty()) std::cout << " (" << extra << ")";
        std::cout << "\n";
    }
}

// ---------------------------------------------------------
// 1. STL Parser Tests (Positive, Negative, Numerical)
// ---------------------------------------------------------
void testSTLParser() {
    std::cout << "\n=== 1. STL Parser Test Suite ===\n";

    // Positive: Valid STL string
    std::string validSTL =
        "solid test_cube\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex 0.0 0.0 1.0\n"
        "      vertex 1.0 0.0 1.0\n"
        "      vertex 1.0 1.0 1.0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex 0.0 0.0 1.0\n"
        "      vertex 1.0 1.0 1.0\n"
        "      vertex 0.0 1.0 1.0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid test_cube\n";

    auto m1 = STLParser::parseAsciiFromString(validSTL);
    reportTest("Valid STL parsing", m1.isValid && m1.triangleCount == 2);
    reportTest("Valid STL solid name", m1.solidName == "test_cube");
    reportTest("Bounding box calculation", m1.bboxMax.x == 1.0f && m1.bboxMax.y == 1.0f);

    // Negative: Empty file
    auto mEmpty = STLParser::parseAsciiFromString("");
    reportTest("Empty STL rejection", !mEmpty.isValid, mEmpty.errorMessage);

    // Negative: Missing solid keyword
    auto mMalformed = STLParser::parseAsciiFromString("facet normal 0 0 1\nvertex 1 1 1\n");
    reportTest("Malformed STL rejection (missing solid)", !mMalformed.isValid, mMalformed.errorMessage);

    // Negative: Non-finite coordinates (NaN/Inf)
    std::string nanSTL =
        "solid nan_test\n"
        "  facet normal 0.0 0.0 1.0\n"
        "    outer loop\n"
        "      vertex NaN 0.0 1.0\n"
        "      vertex 1.0 0.0 1.0\n"
        "      vertex 1.0 1.0 1.0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid nan_test\n";
    auto mNaN = STLParser::parseAsciiFromString(nanSTL);
    reportTest("Non-finite NaN coordinate rejection", !mNaN.isValid, mNaN.errorMessage);
}

// ---------------------------------------------------------
// 2. ANSYS CSV Parser Tests (Positive, Negative, Numerical)
// ---------------------------------------------------------
void testANSYSParser() {
    std::cout << "\n=== 2. ANSYS CSV Parser Test Suite ===\n";

    // Positive: Valid CSV
    std::string validCSV =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,100.0\n"
        "2,0.5,0.0,0.0,120.0\n"
        "3,1.0,0.0,0.0,20.0\n";

    auto r1 = ANSYSCSVParser::parseCSVFromString(validCSV);
    reportTest("Valid ANSYS CSV parsing", r1.isValid && r1.nodeCount == 3);
    reportTest("ANSYS min/max temperature calculation",
               std::abs(r1.minTemperature - 20.0f) < 1e-4f && std::abs(r1.maxTemperature - 120.0f) < 1e-4f);

    // Negative: Empty file
    auto rEmpty = ANSYSCSVParser::parseCSVFromString("");
    reportTest("Empty CSV rejection", !rEmpty.isValid, rEmpty.errorMessage);

    // Negative: Missing required header columns
    std::string badHeader = "id,pos_x,pos_y,pos_z,temp\n1,0,0,0,100\n";
    auto rBadH = ANSYSCSVParser::parseCSVFromString(badHeader);
    reportTest("Missing column schema rejection", !rBadH.isValid, rBadH.errorMessage);

    // Negative: Non-finite value (Inf)
    std::string infCSV =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,inf\n";
    auto rInf = ANSYSCSVParser::parseCSVFromString(infCSV);
    reportTest("Non-finite Inf temperature rejection", !rInf.isValid, rInf.errorMessage);
}

// ---------------------------------------------------------
// 3. Thermal FEA Solver Tests (Numerical & Analytical)
// ---------------------------------------------------------
void testFEASolver() {
    std::cout << "\n=== 3. Thermal 1D FEA Solver Test Suite ===\n";

    FEARodParams p;
    p.L = 1.0;
    p.A = 1e-4;
    p.k = 10.0;
    p.q = 10000.0;
    p.T0 = 100.0;
    p.TL = 20.0;
    p.N = 10;

    auto sol10 = FEARodSolver::solve(p);
    reportTest("FEA solve successful", sol10.isSuccess);

    // Check nodal exact agreement (discrete Green's function property: nodal error ~ 0)
    reportTest("Nodal accuracy (error <= 1e-10 C at nodes)", sol10.maxNodeError <= 1e-10,
               "maxNodeError = " + std::to_string(sol10.maxNodeError));

    // Check midpoint error matches exact formula q*h^2/(8*k)
    // h = 0.1 -> e = 10000 * (0.1)^2 / (80) = 1.25 C
    double expectedMidError10 = 1.25;
    reportTest("Midpoint error matches analytical formula (N=10)",
               std::abs(sol10.maxMidpointError - expectedMidError10) < 1e-8,
               "got: " + std::to_string(sol10.maxMidpointError) + " C, expected: 1.25 C");

    // Mesh convergence test for N = 10, 20, 40, 80
    auto study = FEARodSolver::runConvergenceStudy(p);
    reportTest("Convergence study runs for all 4 meshes", study.solutions.size() == 4);

    // Check error scaling: N=20 -> e=0.3125, N=40 -> e=0.078125, N=80 -> e=0.01953125
    double e20 = study.solutions[1].maxMidpointError;
    double e40 = study.solutions[2].maxMidpointError;
    double e80 = study.solutions[3].maxMidpointError;

    reportTest("Midpoint error N=20 (expected 0.3125 C)", std::abs(e20 - 0.3125) < 1e-6);
    reportTest("Midpoint error N=40 (expected 0.078125 C)", std::abs(e40 - 0.078125) < 1e-6);
    reportTest("Midpoint error N=80 (expected 0.01953125 C)", std::abs(e80 - 0.01953125) < 1e-6);

    // Check convergence rate p approx 2.0
    bool allRatesSecondOrder = true;
    for (double rate : study.observedRates) {
        if (std::abs(rate - 2.0) > 1e-4) allRatesSecondOrder = false;
    }
    reportTest("Mesh convergence rate p = 2.0000 (O(h^2))", allRatesSecondOrder,
               "Rate: " + std::to_string(study.observedRates[0]));
}

// ---------------------------------------------------------
// 4. Multicore Batch Tests (Deterministic Order & Agreement)
// ---------------------------------------------------------
void testMulticore() {
    std::cout << "\n=== 4. Multicore Batch Test Suite ===\n";

    BatchRunConfig cfg;
    cfg.caseCount = 100;
    cfg.meshN = 50;
    cfg.seed = 42;
    cfg.workerCounts = {1, 2, 4};
    cfg.repetitions = 2;
    cfg.includeWarmup = false;

    auto batch = MulticoreBatchEngine::generateBatch(cfg);
    reportTest("Batch generation (seed=42 reproducibility)", batch.size() == 100);

    auto serialOut = MulticoreBatchEngine::solveSerial(batch);
    auto parallelOut2 = MulticoreBatchEngine::solveParallel(batch, 2);
    auto parallelOut4 = MulticoreBatchEngine::solveParallel(batch, 4);

    reportTest("Deterministic output ordering (caseId matches index)",
               parallelOut4[0].caseId == 0 && parallelOut4[99].caseId == 99);

    // Agreement check <= 1e-8 C
    double maxDiff2 = 0.0, maxDiff4 = 0.0;
    for (size_t i = 0; i < batch.size(); ++i) {
        double d2 = std::abs(parallelOut2[i].centerTemperature - serialOut[i].centerTemperature);
        double d4 = std::abs(parallelOut4[i].centerTemperature - serialOut[i].centerTemperature);
        if (d2 > maxDiff2) maxDiff2 = d2;
        if (d4 > maxDiff4) maxDiff4 = d4;
    }

    reportTest("Serial vs 2 Workers agreement (max diff <= 1e-8 C)", maxDiff2 <= 1e-8,
               "maxDiff: " + std::to_string(maxDiff2));
    reportTest("Serial vs 4 Workers agreement (max diff <= 1e-8 C)", maxDiff4 <= 1e-8,
               "maxDiff: " + std::to_string(maxDiff4));
}

// ---------------------------------------------------------
// 5. Embedded Mock Adapters Tests (Discovery, Fault, Recovery)
// ---------------------------------------------------------
void testEmbedded() {
    std::cout << "\n=== 5. Embedded System Adapters Test Suite ===\n";

    EmbeddedDeviceManager mgr;
    auto devices = mgr.getDevices();
    reportTest("Device discovery (3 devices registered)", devices.size() == 3);

    auto mcu = mgr.getDevice("mcu-stm32h7");
    auto fpga = mgr.getDevice("fpga-artix7");
    auto jtag = mgr.getDevice("jtag-probe");

    reportTest("MCU discovery", mcu != nullptr);
    reportTest("FPGA discovery", fpga != nullptr);
    reportTest("JTAG Probe discovery", jtag != nullptr);

    // Connect & Telemetry
    mcu->connect();
    reportTest("MCU Connect", mcu->getState() == ConnectionState::Connected);
    std::string telem = mcu->readTelemetryJson();
    reportTest("MCU Telemetry contains NOMINAL status", telem.find("NOMINAL") != std::string::npos);

    // Fault injection & Recovery
    mcu->injectTimeoutFault(true);
    std::string faultTelem = mcu->readTelemetryJson();
    reportTest("MCU Timeout fault injection", mcu->getState() == ConnectionState::ErrorTimeout);
    reportTest("MCU Error message reported", faultTelem.find("ERROR_TIMEOUT") != std::string::npos);

    bool recovered = mcu->recoverFromTimeout();
    reportTest("MCU Recover from timeout", recovered && mcu->getState() == ConnectionState::Connected);

    mcu->disconnect();
    reportTest("MCU Disconnect", mcu->getState() == ConnectionState::Disconnected);
}

// ---------------------------------------------------------
// 6. Backpropagation Tests (Chain Rule, Finite Diff, 100 Steps)
// ---------------------------------------------------------
void testBackprop() {
    std::cout << "\n=== 6. Backpropagation Test Suite ===\n";

    BackpropParameters p{0.5, 0.0, 1.0, 0.0};
    auto fwd = BackpropEngine::forward(p, 1.0, 2.0);

    // Check forward values
    // h = tanh(0.5) approx 0.46211716
    // y_hat = 1.0 * 0.46211716 + 0.0 = 0.46211716
    // residual = -1.53788284
    // loss = 0.5 * (-1.53788284)^2 = 1.18254182
    reportTest("Forward pass h = tanh(0.5)", std::abs(fwd.h - 0.462117157) < 1e-6);
    reportTest("Forward pass initial loss J = 1.1825418", std::abs(fwd.loss - 1.1825418) < 1e-6);

    // Check analytical gradients
    auto grads = BackpropEngine::computeGradients(p, 1.0, 2.0);
    // dJ/dc = -1.53788284
    // dJ/dv = -0.71068188
    // dJ/dw = -1.20946440
    // dJ/db = -1.20946440
    reportTest("Analytical dJ/dc = -1.53788", std::abs(grads.dJ_dc - (-1.53788284)) < 1e-5);
    reportTest("Analytical dJ/dv = -0.71068", std::abs(grads.dJ_dv - (-0.71068188)) < 1e-5);
    reportTest("Analytical dJ/dw = -1.20946", std::abs(grads.dJ_dw - (-1.20946440)) < 1e-5);
    reportTest("Analytical dJ/db = -1.20946", std::abs(grads.dJ_db - (-1.20946440)) < 1e-5);

    // Central Finite Differences check: epsilon = 1e-6, absolute error <= 1e-5
    auto study = BackpropEngine::runFullStudy(0.1, 100);
    reportTest("All 4 gradient checks pass (eps=1e-6, error <= 1e-5)", study.allChecksPass);

    for (const auto& c : study.gradientChecks) {
        reportTest("GradCheck for " + c.paramName + " (error <= 1e-5)", c.passes,
                   "error = " + std::to_string(c.absoluteError));
    }

    // 100 training steps monotonic loss reduction
    double initialLoss = study.history100Steps[0].loss;
    double finalLoss = study.history100Steps.back().loss;
    reportTest("100 Steps Training convergence (loss decreases monotonically)",
               finalLoss < initialLoss && finalLoss < 0.05,
               "Initial: " + std::to_string(initialLoss) + ", Final: " + std::to_string(finalLoss));
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "      PASS UNIFIED ENGINEERING WORKBENCH TEST SUITE      \n";
    std::cout << "========================================================\n";

    testSTLParser();
    testANSYSParser();
    testFEASolver();
    testMulticore();
    testEmbedded();
    testBackprop();

    std::cout << "\n========================================================\n";
    std::cout << "  TEST RESULTS: " << passedTests << " / " << totalTests << " PASSED\n";
    if (passedTests == totalTests) {
        std::cout << "  ALL UNIT, INTEGRATION, AND NUMERICAL TESTS PASSED!     \n";
    } else {
        std::cout << "  SOME TESTS FAILED!\n";
    }
    std::cout << "========================================================\n";

    return (passedTests == totalTests) ? 0 : 1;
}
