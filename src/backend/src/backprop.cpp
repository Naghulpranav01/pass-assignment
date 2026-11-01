#include "backprop.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace Workbench {

double BackpropEngine::computeLoss(const BackpropParameters& p, double x, double y) {
    double z = p.w * x + p.b;
    double h = std::tanh(z);
    double y_hat = p.v * h + p.c;
    double residual = y_hat - y;
    return 0.5 * residual * residual;
}

ForwardPassResult BackpropEngine::forward(const BackpropParameters& p, double x, double y) {
    ForwardPassResult res;
    res.x = x;
    res.y = y;
    res.z = p.w * x + p.b;
    res.h = std::tanh(res.z);
    res.y_hat = p.v * res.h + p.c;
    res.residual = res.y_hat - y;
    res.loss = 0.5 * res.residual * res.residual;
    return res;
}

BackpropGradients BackpropEngine::computeGradients(const BackpropParameters& p, double x, double y) {
    auto fwd = forward(p, x, y);

    // dJ/dy_hat = y_hat - y
    double dJ_dyhat = fwd.residual;

    // dJ/dc = dJ/dyhat * dyhat/dc = dJ_dyhat * 1
    double dJ_dc = dJ_dyhat;

    // dJ/dv = dJ/dyhat * dyhat/dv = dJ_dyhat * h
    double dJ_dv = dJ_dyhat * fwd.h;

    // dJ/dh = dJ/dyhat * dyhat/dh = dJ_dyhat * v
    double dJ_dh = dJ_dyhat * p.v;

    // dh/dz = 1 - tanh^2(z) = 1 - h^2
    double dh_dz = 1.0 - fwd.h * fwd.h;

    // dJ/dz = dJ/dh * dh/dz
    double dJ_dz = dJ_dh * dh_dz;

    // dJ/dw = dJ/dz * dz/dw = dJ_dz * x
    double dJ_dw = dJ_dz * x;

    // dJ/db = dJ/dz * dz/db = dJ_dz * 1
    double dJ_db = dJ_dz * 1.0;

    return {dJ_dw, dJ_db, dJ_dv, dJ_dc};
}

BackpropGradients BackpropEngine::computeNumericalGradients(
    const BackpropParameters& p, double x, double y, double eps) {

    BackpropGradients numGrads;

    // w
    {
        BackpropParameters pPlus = p; pPlus.w += eps;
        BackpropParameters pMinus = p; pMinus.w -= eps;
        numGrads.dJ_dw = (computeLoss(pPlus, x, y) - computeLoss(pMinus, x, y)) / (2.0 * eps);
    }
    // b
    {
        BackpropParameters pPlus = p; pPlus.b += eps;
        BackpropParameters pMinus = p; pMinus.b -= eps;
        numGrads.dJ_db = (computeLoss(pPlus, x, y) - computeLoss(pMinus, x, y)) / (2.0 * eps);
    }
    // v
    {
        BackpropParameters pPlus = p; pPlus.v += eps;
        BackpropParameters pMinus = p; pMinus.v -= eps;
        numGrads.dJ_dv = (computeLoss(pPlus, x, y) - computeLoss(pMinus, x, y)) / (2.0 * eps);
    }
    // c
    {
        BackpropParameters pPlus = p; pPlus.c += eps;
        BackpropParameters pMinus = p; pMinus.c -= eps;
        numGrads.dJ_dc = (computeLoss(pPlus, x, y) - computeLoss(pMinus, x, y)) / (2.0 * eps);
    }

    return numGrads;
}

BackpropReport BackpropEngine::runFullStudy(double lr, int numSteps) {
    BackpropReport report;
    BackpropParameters p{0.5, 0.0, 1.0, 0.0};
    double x = 1.0;
    double y = 2.0;

    report.initialForward = forward(p, x, y);
    report.initialLoss = report.initialForward.loss;
    report.initialAnalyticalGrads = computeGradients(p, x, y);
    auto numGrads = computeNumericalGradients(p, x, y, 1e-6);

    auto checkGrad = [](const std::string& name, double ana, double num) {
        GradientCheckResult r;
        r.paramName = name;
        r.analyticalGrad = ana;
        r.numericalGrad = num;
        r.absoluteError = std::abs(ana - num);
        r.passes = (r.absoluteError <= 1e-5); // Stated requirement <= 1e-5
        return r;
    };

    report.gradientChecks.push_back(checkGrad("w", report.initialAnalyticalGrads.dJ_dw, numGrads.dJ_dw));
    report.gradientChecks.push_back(checkGrad("b", report.initialAnalyticalGrads.dJ_db, numGrads.dJ_db));
    report.gradientChecks.push_back(checkGrad("v", report.initialAnalyticalGrads.dJ_dv, numGrads.dJ_dv));
    report.gradientChecks.push_back(checkGrad("c", report.initialAnalyticalGrads.dJ_dc, numGrads.dJ_dc));

    for (const auto& c : report.gradientChecks) {
        if (!c.passes) report.allChecksPass = false;
    }

    // Step 1 Simultaneous Update
    report.updatedParamsStep1.w = p.w - lr * report.initialAnalyticalGrads.dJ_dw;
    report.updatedParamsStep1.b = p.b - lr * report.initialAnalyticalGrads.dJ_db;
    report.updatedParamsStep1.v = p.v - lr * report.initialAnalyticalGrads.dJ_dv;
    report.updatedParamsStep1.c = p.c - lr * report.initialAnalyticalGrads.dJ_dc;

    // 100 Training steps
    BackpropParameters currentP = p;
    report.history100Steps.reserve(numSteps + 1);

    for (int step = 0; step <= numSteps; ++step) {
        auto fwd = forward(currentP, x, y);
        auto grads = computeGradients(currentP, x, y);

        TrainingStepRecord rec;
        rec.step = step;
        rec.loss = fwd.loss;
        rec.y_hat = fwd.y_hat;
        rec.params = currentP;
        rec.grads = grads;
        report.history100Steps.push_back(rec);

        if (step < numSteps) {
            // Simultaneous update
            currentP.w -= lr * grads.dJ_dw;
            currentP.b -= lr * grads.dJ_db;
            currentP.v -= lr * grads.dJ_dv;
            currentP.c -= lr * grads.dJ_dc;
        }
    }

    return report;
}

std::string BackpropReport::toCSV() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "# Backpropagation 100 Steps Training History (lr=0.1, x=1.0, y=2.0)\n";
    ss << "step,loss,y_hat,w,b,v,c,dJ_dw,dJ_db,dJ_dv,dJ_dc\n";
    for (const auto& r : history100Steps) {
        ss << r.step << "," << r.loss << "," << r.y_hat << ","
           << r.params.w << "," << r.params.b << "," << r.params.v << "," << r.params.c << ","
           << r.grads.dJ_dw << "," << r.grads.dJ_db << "," << r.grads.dJ_dv << "," << r.grads.dJ_dc << "\n";
    }
    return ss.str();
}

std::string BackpropReport::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "{\n";
    ss << "  \"initialForward\": {\n";
    ss << "    \"x\": " << initialForward.x << ",\n";
    ss << "    \"y\": " << initialForward.y << ",\n";
    ss << "    \"z\": " << initialForward.z << ",\n";
    ss << "    \"h\": " << initialForward.h << ",\n";
    ss << "    \"y_hat\": " << initialForward.y_hat << ",\n";
    ss << "    \"residual\": " << initialForward.residual << ",\n";
    ss << "    \"loss\": " << initialForward.loss << "\n";
    ss << "  },\n";
    ss << "  \"gradientChecks\": [\n";
    for (size_t i = 0; i < gradientChecks.size(); ++i) {
        const auto& g = gradientChecks[i];
        ss << "    {\"param\":\"" << g.paramName << "\",\"analytical\":" << g.analyticalGrad
           << ",\"numerical\":" << g.numericalGrad << ",\"absError\":" << g.absoluteError
           << ",\"passed\":" << (g.passes ? "true" : "false") << "}";
        if (i + 1 < gradientChecks.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"step1UpdatedParams\": {\n";
    ss << "    \"w\": " << updatedParamsStep1.w << ",\n";
    ss << "    \"b\": " << updatedParamsStep1.b << ",\n";
    ss << "    \"v\": " << updatedParamsStep1.v << ",\n";
    ss << "    \"c\": " << updatedParamsStep1.c << "\n";
    ss << "  },\n";
    ss << "  \"history\": [\n";
    for (size_t i = 0; i < history100Steps.size(); ++i) {
        const auto& h = history100Steps[i];
        ss << "    {\"step\":" << h.step << ",\"loss\":" << h.loss
           << ",\"y_hat\":" << h.y_hat
           << ",\"w\":" << h.params.w << ",\"b\":" << h.params.b
           << ",\"v\":" << h.params.v << ",\"c\":" << h.params.c << "}";
        if (i + 1 < history100Steps.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace Workbench
