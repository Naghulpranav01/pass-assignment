#include "fea_1d_solver.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Workbench {

double FEARodSolver::exactTemperature(double x, const FEARodParams& p) {
    // T_exact(x) = 100 - 80*x/L + [q/(2k)]*x*(L-x)
    // generalized for T0 and TL: T0 + (TL - T0)*x/L + (q / (2.0 * k)) * x * (L - x)
    return p.T0 + (p.TL - p.T0) * (x / p.L) + (p.q / (2.0 * p.k)) * x * (p.L - x);
}

FEARodSolution FEARodSolver::solve(const FEARodParams& p) {
    FEARodSolution sol;
    sol.params = p;

    if (p.N < 1) {
        sol.isSuccess = false;
        sol.errorMessage = "Element count N must be >= 1.";
        return sol;
    }
    if (p.L <= 0.0 || p.A <= 0.0 || p.k <= 0.0) {
        sol.isSuccess = false;
        sol.errorMessage = "Physical parameters L, A, and k must be strictly positive.";
        return sol;
    }

    const int N = p.N;
    const int numNodes = N + 1;
    const double h = p.L / N;
    sol.h = h;

    // Element stiffness: K_elem = (k*A/h) * [1, -1; -1, 1]
    const double ke = (p.k * p.A) / h;
    // Element load: F_elem = (q*A*h/2) * [1; 1]
    const double fe = (p.q * p.A * h) * 0.5;

    // Global system: Tridiagonal matrix
    // diag[i], sub[i] (lower, sub[0] unused), sup[i] (upper, sup[numNodes-1] unused), rhs[i]
    std::vector<double> diag(numNodes, 0.0);
    std::vector<double> sub(numNodes, 0.0);
    std::vector<double> sup(numNodes, 0.0);
    std::vector<double> rhs(numNodes, 0.0);

    // Assembly loop over elements
    for (int e = 0; e < N; ++e) {
        int n1 = e;
        int n2 = e + 1;

        diag[n1] += ke;
        diag[n2] += ke;
        sup[n1]  -= ke;
        sub[n2]  -= ke;

        rhs[n1]  += fe;
        rhs[n2]  += fe;
    }

    // Impose Dirichlet Boundary Conditions: T(0) = T0, T(L) = TL
    // We enforce:
    // T[0] = p.T0
    // For node 1: diag[1]*T1 + sup[1]*T2 = rhs[1] - sub[1]*T0
    // For node N-1: sub[N-1]*T_{N-2} + diag[N-1]*T_{N-1} = rhs[N-1] - sup[N-1]*TL
    // T[N] = p.TL

    std::vector<double> T(numNodes, 0.0);
    T[0] = p.T0;
    T[N] = p.TL;

    if (N == 1) {
        // Single element with both ends fixed: trivial nodes
        // (No interior degrees of freedom)
    } else {
        // Interior degrees of freedom: indices 1 to N-1
        int interiorSize = N - 1;
        std::vector<double> c_diag(interiorSize);
        std::vector<double> c_sub(interiorSize, 0.0);
        std::vector<double> c_sup(interiorSize, 0.0);
        std::vector<double> c_rhs(interiorSize);

        for (int i = 0; i < interiorSize; ++i) {
            int gIdx = i + 1;
            c_diag[i] = diag[gIdx];
            c_rhs[i]  = rhs[gIdx];
            if (i > 0) c_sub[i] = sub[gIdx];
            if (i < interiorSize - 1) c_sup[i] = sup[gIdx];
        }

        // Apply boundary contributions to RHS
        c_rhs[0] -= sub[1] * p.T0;
        c_rhs[interiorSize - 1] -= sup[N - 1] * p.TL;

        // Thomas Algorithm (tridiagonal matrix solver)
        for (int i = 1; i < interiorSize; ++i) {
            double m = c_sub[i] / c_diag[i - 1];
            c_diag[i] -= m * c_sup[i - 1];
            c_rhs[i]  -= m * c_rhs[i - 1];
        }

        std::vector<double> x_int(interiorSize);
        x_int[interiorSize - 1] = c_rhs[interiorSize - 1] / c_diag[interiorSize - 1];
        for (int i = interiorSize - 2; i >= 0; --i) {
            x_int[i] = (c_rhs[i] - c_sup[i] * x_int[i + 1]) / c_diag[i];
        }

        for (int i = 0; i < interiorSize; ++i) {
            T[i + 1] = x_int[i];
        }
    }

    // Record nodal results and compute errors
    double maxNErr = 0.0;
    sol.nodes.resize(numNodes);
    for (int i = 0; i < numNodes; ++i) {
        double x = i * h;
        double t_exact = exactTemperature(x, p);
        double err = std::abs(T[i] - t_exact);
        if (err > maxNErr) maxNErr = err;

        sol.nodes[i] = {i, x, T[i], t_exact, err};
    }
    sol.maxNodeError = maxNErr;

    // Record midpoint results using FE interpolation:
    // T_fea(x_mid) = 0.5 * (T_i + T_{i+1})
    double maxMErr = 0.0;
    sol.midpoints.resize(N);
    for (int e = 0; e < N; ++e) {
        double x_mid = (e + 0.5) * h;
        double t_fea_mid = 0.5 * (T[e] + T[e + 1]);
        double t_exact_mid = exactTemperature(x_mid, p);
        double err = std::abs(t_fea_mid - t_exact_mid);
        if (err > maxMErr) maxMErr = err;

        sol.midpoints[e] = {e, x_mid, t_fea_mid, t_exact_mid, err};
    }
    sol.maxMidpointError = maxMErr;
    // Theoretical midpoint error: e = q * h^2 / (8 * k)
    sol.theoreticalMidpointError = (p.q * h * h) / (8.0 * p.k);

    sol.isSuccess = true;
    return sol;
}

MeshConvergenceStudy FEARodSolver::runConvergenceStudy(FEARodParams baseParams) {
    MeshConvergenceStudy study;
    study.baseParams = baseParams;

    for (int N : study.elementCounts) {
        FEARodParams p = baseParams;
        p.N = N;
        study.solutions.push_back(solve(p));
    }

    // Compute convergence rates p = log(E1/E2) / log(h1/h2)
    study.observedRates.clear();
    for (size_t i = 1; i < study.solutions.size(); ++i) {
        double h1 = study.solutions[i - 1].h;
        double h2 = study.solutions[i].h;
        double e1 = study.solutions[i - 1].maxMidpointError;
        double e2 = study.solutions[i].maxMidpointError;
        if (e1 > 1e-15 && e2 > 1e-15) {
            double rate = std::log(e1 / e2) / std::log(h1 / h2);
            study.observedRates.push_back(rate);
        } else {
            study.observedRates.push_back(0.0);
        }
    }

    return study;
}

std::string FEARodSolution::toCSV() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "# 1D Thermal FEA Rod Simulation Results\n";
    ss << "# Parameters: L=" << params.L << " m, A=" << params.A << " m2, k=" << params.k
       << " W/(m K), q=" << params.q << " W/m3, T0=" << params.T0 << " C, TL=" << params.TL
       << " C, N=" << params.N << " elements (h=" << h << " m)\n";
    ss << "type,index,x_m,T_fea_C,T_exact_C,error_abs_C\n";

    for (const auto& n : nodes) {
        ss << "node," << n.nodeIndex << "," << n.x << "," << n.T_fea << "," << n.T_exact << "," << n.absoluteError << "\n";
    }
    for (const auto& m : midpoints) {
        ss << "midpoint," << m.elementIndex << "," << m.x_mid << "," << m.T_fea_mid << "," << m.T_exact_mid << "," << m.absoluteError << "\n";
    }
    return ss.str();
}

std::string FEARodSolution::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "{\n";
    ss << "  \"success\": " << (isSuccess ? "true" : "false") << ",\n";
    ss << "  \"N\": " << params.N << ",\n";
    ss << "  \"h\": " << h << ",\n";
    ss << "  \"maxNodeError\": " << maxNodeError << ",\n";
    ss << "  \"maxMidpointError\": " << maxMidpointError << ",\n";
    ss << "  \"theoreticalMidpointError\": " << theoreticalMidpointError << ",\n";
    ss << "  \"nodes\": [\n";
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        ss << "    {\"idx\":" << n.nodeIndex << ",\"x\":" << n.x
           << ",\"t_fea\":" << n.T_fea << ",\"t_exact\":" << n.T_exact
           << ",\"err\":" << n.absoluteError << "}";
        if (i + 1 < nodes.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"midpoints\": [\n";
    for (size_t i = 0; i < midpoints.size(); ++i) {
        const auto& m = midpoints[i];
        ss << "    {\"elem\":" << m.elementIndex << ",\"x\":" << m.x_mid
           << ",\"t_fea\":" << m.T_fea_mid << ",\"t_exact\":" << m.T_exact_mid
           << ",\"err\":" << m.absoluteError << "}";
        if (i + 1 < midpoints.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

std::string MeshConvergenceStudy::toConvergenceCSV() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "N_elements,h_m,max_node_error_C,max_midpoint_error_C,theoretical_midpoint_error_C,convergence_rate\n";
    for (size_t i = 0; i < solutions.size(); ++i) {
        const auto& sol = solutions[i];
        double rate = (i == 0) ? 0.0 : observedRates[i - 1];
        ss << sol.params.N << "," << sol.h << "," << sol.maxNodeError << ","
           << sol.maxMidpointError << "," << sol.theoreticalMidpointError << "," << rate << "\n";
    }
    return ss.str();
}

std::string MeshConvergenceStudy::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "{\n";
    ss << "  \"studies\": [\n";
    for (size_t i = 0; i < solutions.size(); ++i) {
        const auto& sol = solutions[i];
        double rate = (i == 0) ? 0.0 : observedRates[i - 1];
        ss << "    {\n";
        ss << "      \"N\": " << sol.params.N << ",\n";
        ss << "      \"h\": " << sol.h << ",\n";
        ss << "      \"maxNodeError\": " << sol.maxNodeError << ",\n";
        ss << "      \"maxMidpointError\": " << sol.maxMidpointError << ",\n";
        ss << "      \"theoreticalMidpointError\": " << sol.theoreticalMidpointError << ",\n";
        ss << "      \"observedRate\": " << rate << ",\n";
        ss << "      \"nodes\": [\n";
        for (size_t j = 0; j < sol.nodes.size(); ++j) {
            const auto& n = sol.nodes[j];
            ss << "        {\"x\":" << n.x << ",\"t_fea\":" << n.T_fea << ",\"t_exact\":" << n.T_exact << "}";
            if (j + 1 < sol.nodes.size()) ss << ",";
            ss << "\n";
        }
        ss << "      ]\n";
        ss << "    }";
        if (i + 1 < solutions.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace Workbench
