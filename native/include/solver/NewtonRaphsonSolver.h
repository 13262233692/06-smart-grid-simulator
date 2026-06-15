#pragma once
#include "core/GridData.h"
#include <vector>

namespace GridSolver {

struct SolverConfig {
    double tolerance = 1e-6;
    int maxIterations = 50;
    bool enforceQlimits = true;
};

class NewtonRaphsonSolver {
public:
    NewtonRaphsonSolver();
    ~NewtonRaphsonSolver();

    PowerFlowSolution solve(const GridData& grid, const SolverConfig& config);

    PowerFlowSolution solveWithModifications(
        const GridData& grid,
        const SolverConfig& config,
        const std::vector<std::pair<int, double>>& genPowerChanges,
        const std::vector<std::pair<int, double>>& transformerTapChanges
    );

private:
    using Matrix = std::vector<std::vector<double>>;
    using Vector = std::vector<double>;

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
};

} // namespace GridSolver
