#pragma once

#include <vector>
#include <string>

namespace Workbench {

struct FEARodParams {
    double L{1.0};          // Length [m]
    double A{1e-4};         // Cross-sectional area [m^2]
    double k{10.0};         // Thermal conductivity [W/(m K)]
    double q{10000.0};      // Volumetric heat generation [W/m^3]
    double T0{100.0};       // Left prescribed temperature [deg C]
    double TL{20.0};        // Right prescribed temperature [deg C]
    int N{10};              // Number of uniform elements
};

struct FEANodeResult {
    int nodeIndex{0};
    double x{0.0};
    double T_fea{0.0};
    double T_exact{0.0};
    double absoluteError{0.0};
};

struct FEAMidpointResult {
    int elementIndex{0};
    double x_mid{0.0};
    double T_fea_mid{0.0};
    double T_exact_mid{0.0};
    double absoluteError{0.0};
};

struct FEARodSolution {
    FEARodParams params;
    double h{0.0};          // Element length [m]
    std::vector<FEANodeResult> nodes;
    std::vector<FEAMidpointResult> midpoints;
    double maxNodeError{0.0};
    double maxMidpointError{0.0};
    double theoreticalMidpointError{0.0};
    bool isSuccess{true};
    std::string errorMessage;

    std::string toCSV() const;
    std::string toJson() const;
};

struct MeshConvergenceStudy {
    FEARodParams baseParams;
    std::vector<int> elementCounts{10, 20, 40, 80};
    std::vector<FEARodSolution> solutions;
    std::vector<double> observedRates;

    std::string toConvergenceCSV() const;
    std::string toJson() const;
};

class FEARodSolver {
public:
    static double exactTemperature(double x, const FEARodParams& p);
    static FEARodSolution solve(const FEARodParams& params);
    static MeshConvergenceStudy runConvergenceStudy(FEARodParams baseParams = FEARodParams());
};

} // namespace Workbench
