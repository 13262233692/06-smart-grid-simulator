#pragma once
#include "core/GridData.h"
#include "solver/ComputationGuard.h"
#include <vector>
#include <mutex>

namespace GridSolver {

struct SolverConfig {
    double tolerance = 1e-6;
    int maxIterations = 50;
    bool enforceQlimits = true;
    double timeoutMs = 30000;
};

class NewtonRaphsonSolver {
public:
    NewtonRaphsonSolver();
    ~NewtonRaphsonSolver();

    PowerFlowSolution solve(
        const GridData& grid,
        const SolverConfig& config,
        CancellationToken* token = nullptr,
        ProgressCallback progress = nullptr
    );

    PowerFlowSolution solveWithModifications(
        const GridData& grid,
        const SolverConfig& config,
        const std::vector<std::pair<int, double>>& genPowerChanges,
        const std::vector<std::pair<int, double>>& transformerTapChanges,
        CancellationToken* token = nullptr,
        ProgressCallback progress = nullptr
    );

    SolverState& state() { return m_state; }

private:
    using Matrix = std::vector<std::vector<double>>;
    using Vector = std::vector<double>;

    SolverState m_state;

    void buildYbus(const GridData& grid, std::vector<std::vector<Complex>>& Ybus);
    void buildJacobian(
        const GridData& grid,
        const std::vector<Bus>& buses,
        const std::vector<std::vector<Complex>>& Ybus,
        Matrix& J,
        Vector& mismatch
    );

    bool solveLinearSystem(Matrix& A, Vector& b, Vector& x);
    bool luDecomposition(Matrix& A, std::vector<int>& pivot);
    void luBackSubstitution(const Matrix& LU, const std::vector<int>& pivot, const Vector& b, Vector& x);

    void calculateBranchFlows(
        const GridData& grid,
        const std::vector<Bus>& buses,
        const std::vector<std::vector<Complex>>& Ybus,
        PowerFlowSolution& solution
    );

    bool checkToken(CancellationToken* token) const;
    void reportProgress(ProgressCallback progress, int iter, int maxIter, double mismatch, double tol, double elapsed) const;
};

} // namespace GridSolver
