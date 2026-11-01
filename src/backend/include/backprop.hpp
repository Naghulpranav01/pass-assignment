#pragma once

#include <vector>
#include <string>

namespace Workbench {

struct BackpropParameters {
    double w{0.5};
    double b{0.0};
    double v{1.0};
    double c{0.0};
};

struct BackpropGradients {
    double dJ_dw{0.0};
    double dJ_db{0.0};
    double dJ_dv{0.0};
    double dJ_dc{0.0};
};

struct ForwardPassResult {
    double x{1.0};
    double y{2.0};
    double z{0.0};
    double h{0.0};
    double y_hat{0.0};
    double residual{0.0};
    double loss{0.0};
};

struct GradientCheckResult {
    std::string paramName;
    double analyticalGrad{0.0};
    double numericalGrad{0.0};
    double absoluteError{0.0};
    bool passes{true}; // Stated requirement: <= 1e-5 with epsilon = 1e-6
};

struct TrainingStepRecord {
    int step{0};
    double loss{0.0};
    double y_hat{0.0};
    BackpropParameters params;
    BackpropGradients grads;
};

struct BackpropReport {
    double initialLoss{0.0};
    ForwardPassResult initialForward;
    BackpropGradients initialAnalyticalGrads;
    std::vector<GradientCheckResult> gradientChecks;
    BackpropParameters updatedParamsStep1;
    std::vector<TrainingStepRecord> history100Steps;
    bool allChecksPass{true};

    std::string toCSV() const;
    std::string toJson() const;
};

class BackpropEngine {
public:
    static double computeLoss(const BackpropParameters& p, double x = 1.0, double y = 2.0);
    static ForwardPassResult forward(const BackpropParameters& p, double x = 1.0, double y = 2.0);
    static BackpropGradients computeGradients(const BackpropParameters& p, double x = 1.0, double y = 2.0);
    static BackpropGradients computeNumericalGradients(const BackpropParameters& p, double x = 1.0, double y = 2.0, double eps = 1e-6);
    static BackpropReport runFullStudy(double lr = 0.1, int numSteps = 100);
};

} // namespace Workbench
