#include "solver/NewtonRaphsonSolver.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GridSolver {

NewtonRaphsonSolver::NewtonRaphsonSolver() {}
NewtonRaphsonSolver::~NewtonRaphsonSolver() {}

bool NewtonRaphsonSolver::checkToken(CancellationToken* token) const {
    if (!token) return false;
    return token->shouldStop();
}

void NewtonRaphsonSolver::reportProgress(
    ProgressCallback progress, int iter, int maxIter,
    double mismatch, double tol, double elapsed
) const {
    if (!progress) return;
    IterationProgress p;
    p.currentIteration = iter;
    p.maxIterations = maxIter;
    p.maxMismatch = mismatch;
    p.tolerance = tol;
    p.elapsedMs = elapsed;
    progress(p);
}

void NewtonRaphsonSolver::buildYbus(const GridData& grid, std::vector<std::vector<Complex>>& Ybus) {
    int n = grid.nBus();
    Ybus.assign(n, std::vector<Complex>(n, Complex(0, 0)));

    for (const auto& branch : grid.branches) {
        if (branch.status == 0) continue;

        int i = grid.findBusIndex(branch.fromBus);
        int j = grid.findBusIndex(branch.toBus);

        Complex z(branch.r, branch.x);
        Complex y = std::abs(z) > 1e-15 ? Complex(1, 0) / z : Complex(0, 0);
        Complex bShunt(0, branch.b / 2.0);

        double tap = branch.tap;
        if (tap == 0) tap = 1.0;
        double phase = branch.phaseShift;
        Complex tapRatio = std::polar(tap, phase);
        Complex tapConj = std::conj(tapRatio);

        Ybus[i][i] += (y + bShunt) / (tap * tap);
        Ybus[j][j] += y + bShunt;

        Complex offDiag = -y / tapConj;
        Ybus[i][j] += offDiag;
        Ybus[j][i] += std::conj(offDiag);
    }

    for (size_t k = 0; k < grid.buses.size(); k++) {
        const auto& bus = grid.buses[k];
        if (bus.shuntG != 0 || bus.shuntB != 0) {
            Ybus[k][k] += Complex(bus.shuntG / grid.baseMVA, bus.shuntB / grid.baseMVA);
        }
    }
}

void NewtonRaphsonSolver::buildJacobian(
    const GridData& grid,
    const std::vector<Bus>& buses,
    const std::vector<std::vector<Complex>>& Ybus,
    Matrix& J,
    Vector& mismatch
) {
    int n = static_cast<int>(buses.size());

    std::vector<int> nonSlackIdx;
    std::vector<int> pqIdx;

    for (int k = 0; k < n; k++) {
        if (buses[k].type != BusType::SLACK) {
            nonSlackIdx.push_back(k);
        }
        if (buses[k].type == BusType::PQ) {
            pqIdx.push_back(k);
        }
    }

    int nNonSlack = static_cast<int>(nonSlackIdx.size());
    int nPQ = static_cast<int>(pqIdx.size());
    int nUnknown = nNonSlack + nPQ;

    J.assign(nUnknown, Vector(nUnknown, 0.0));
    mismatch.assign(nUnknown, 0.0);

    for (int ri = 0; ri < nNonSlack; ri++) {
        int i = nonSlackIdx[ri];
        double Vi = buses[i].Vm;
        double thetai = buses[i].Va;
        double Pi_spec = (buses[i].Pg - buses[i].Pd) / grid.baseMVA;

        double Pi_calc = 0.0;
        for (int m = 0; m < n; m++) {
            double Vm = buses[m].Vm;
            double G = Ybus[i][m].real();
            double B = Ybus[i][m].imag();
            double dTheta = thetai - buses[m].Va;
            Pi_calc += Vi * Vm * (G * cos(dTheta) + B * sin(dTheta));
        }
        mismatch[ri] = Pi_spec - Pi_calc;
    }

    for (int ri = 0; ri < nPQ; ri++) {
        int i = pqIdx[ri];
        double Vi = buses[i].Vm;
        double thetai = buses[i].Va;
        double Qi_spec = (buses[i].Qg - buses[i].Qd) / grid.baseMVA;

        double Qi_calc = 0.0;
        for (int m = 0; m < n; m++) {
            double Vm = buses[m].Vm;
            double G = Ybus[i][m].real();
            double B = Ybus[i][m].imag();
            double dTheta = thetai - buses[m].Va;
            Qi_calc += Vi * Vm * (G * sin(dTheta) - B * cos(dTheta));
        }
        mismatch[nNonSlack + ri] = Qi_spec - Qi_calc;
    }

    for (int ri = 0; ri < nNonSlack; ri++) {
        int i = nonSlackIdx[ri];
        double Vi = buses[i].Vm;
        double thetai = buses[i].Va;

        for (int ci = 0; ci < nNonSlack; ci++) {
            int j = nonSlackIdx[ci];
            if (i != j) {
                double Vj = buses[j].Vm;
                double dTheta = thetai - buses[j].Va;
                double G = Ybus[i][j].real();
                double B = Ybus[i][j].imag();
                J[ri][ci] = Vi * Vj * (G * sin(dTheta) - B * cos(dTheta));
            } else {
                double sum = 0.0;
                for (int m = 0; m < n; m++) {
                    if (m == i) continue;
                    double Vm = buses[m].Vm;
                    double G = Ybus[i][m].real();
                    double B = Ybus[i][m].imag();
                    double dTheta = thetai - buses[m].Va;
                    sum += Vi * Vm * (G * sin(dTheta) - B * cos(dTheta));
                }
                J[ri][ci] = sum;
            }
        }

        for (int ci = 0; ci < nPQ; ci++) {
            int j = pqIdx[ci];
            int vcol = nNonSlack + ci;
            double Vj = buses[j].Vm;
            double dTheta = thetai - buses[j].Va;
            double G = Ybus[i][j].real();
            double B = Ybus[i][j].imag();

            if (i != j) {
                J[ri][vcol] = Vi * (G * cos(dTheta) + B * sin(dTheta));
            } else {
                double sum = 2.0 * Vi * Ybus[i][i].real();
                for (int m = 0; m < n; m++) {
                    if (m == i) continue;
                    double Vm = buses[m].Vm;
                    double G2 = Ybus[i][m].real();
                    double B2 = Ybus[i][m].imag();
                    double dTheta2 = thetai - buses[m].Va;
                    sum += Vm * (G2 * cos(dTheta2) + B2 * sin(dTheta2));
                }
                J[ri][vcol] = sum;
            }
        }
    }

    for (int ri = 0; ri < nPQ; ri++) {
        int i = pqIdx[ri];
        double Vi = buses[i].Vm;
        double thetai = buses[i].Va;
        int qrow = nNonSlack + ri;

        for (int ci = 0; ci < nNonSlack; ci++) {
            int j = nonSlackIdx[ci];
            if (i != j) {
                double Vj = buses[j].Vm;
                double dTheta = thetai - buses[j].Va;
                double G = Ybus[i][j].real();
                double B = Ybus[i][j].imag();
                J[qrow][ci] = -Vi * Vj * (G * cos(dTheta) + B * sin(dTheta));
            } else {
                double sum = 0.0;
                for (int m = 0; m < n; m++) {
                    if (m == i) continue;
                    double Vm = buses[m].Vm;
                    double G = Ybus[i][m].real();
                    double B = Ybus[i][m].imag();
                    double dTheta = thetai - buses[m].Va;
                    sum += -Vi * Vm * (G * cos(dTheta) + B * sin(dTheta));
                }
                J[qrow][ci] = sum;
            }
        }

        for (int ci = 0; ci < nPQ; ci++) {
            int j = pqIdx[ci];
            int vcol = nNonSlack + ci;
            double Vj = buses[j].Vm;
            double dTheta = thetai - buses[j].Va;
            double G = Ybus[i][j].real();
            double B = Ybus[i][j].imag();

            if (i != j) {
                J[qrow][vcol] = Vi * (G * sin(dTheta) - B * cos(dTheta));
            } else {
                double sum = -2.0 * Vi * Ybus[i][i].imag();
                for (int m = 0; m < n; m++) {
                    if (m == i) continue;
                    double Vm = buses[m].Vm;
                    double G2 = Ybus[i][m].real();
                    double B2 = Ybus[i][m].imag();
                    double dTheta2 = thetai - buses[m].Va;
                    sum += Vm * (G2 * sin(dTheta2) - B2 * cos(dTheta2));
                }
                J[qrow][vcol] = sum;
            }
        }
    }
}

bool NewtonRaphsonSolver::luDecomposition(Matrix& A, std::vector<int>& pivot) {
    int n = static_cast<int>(A.size());
    pivot.resize(n);
    for (int i = 0; i < n; i++) pivot[i] = i;

    for (int k = 0; k < n; k++) {
        int maxRow = k;
        double maxVal = std::abs(A[k][k]);
        for (int i = k + 1; i < n; i++) {
            if (std::abs(A[i][k]) > maxVal) {
                maxVal = std::abs(A[i][k]);
                maxRow = i;
            }
        }

        if (maxVal < 1e-15) return false;

        if (maxRow != k) {
            std::swap(A[k], A[maxRow]);
            std::swap(pivot[k], pivot[maxRow]);
        }

        double pivotVal = A[k][k];
        for (int i = k + 1; i < n; i++) {
            A[i][k] /= pivotVal;
            double factor = A[i][k];
            for (int j = k + 1; j < n; j++) {
                A[i][j] -= factor * A[k][j];
            }
        }
    }

    return true;
}

void NewtonRaphsonSolver::luBackSubstitution(const Matrix& LU, const std::vector<int>& pivot, const Vector& b, Vector& x) {
    int n = static_cast<int>(LU.size());
    x.resize(n);

    for (int i = 0; i < n; i++) {
        x[i] = b[pivot[i]];
    }

    for (int i = 1; i < n; i++) {
        for (int j = 0; j < i; j++) {
            x[i] -= LU[i][j] * x[j];
        }
    }

    for (int i = n - 1; i >= 0; i--) {
        for (int j = i + 1; j < n; j++) {
            x[i] -= LU[i][j] * x[j];
        }
        x[i] /= LU[i][i];
    }
}

bool NewtonRaphsonSolver::solveLinearSystem(Matrix& A, Vector& b, Vector& x) {
    std::vector<int> pivot;
    if (!luDecomposition(A, pivot)) return false;
    luBackSubstitution(A, pivot, b, x);
    return true;
}

void NewtonRaphsonSolver::calculateBranchFlows(
    const GridData& grid,
    const std::vector<Bus>& buses,
    const std::vector<std::vector<Complex>>& Ybus,
    PowerFlowSolution& solution
) {
    solution.totalLossesP = 0.0;
    solution.totalLossesQ = 0.0;

    for (const auto& branch : grid.branches) {
        if (branch.status == 0) continue;

        FlowResult flow;
        flow.fromBus = branch.fromBus;
        flow.toBus = branch.toBus;

        int i = grid.findBusIndex(branch.fromBus);
        int j = grid.findBusIndex(branch.toBus);

        Complex z(branch.r, branch.x);
        Complex y = std::abs(z) > 1e-15 ? Complex(1, 0) / z : Complex(0, 0);
        Complex bShunt(0, branch.b / 2.0);

        double tap = (branch.tap == 0) ? 1.0 : branch.tap;
        double phase = branch.phaseShift;
        Complex tapRatio = std::polar(tap, phase);

        Complex Vi = std::polar(buses[i].Vm, buses[i].Va);
        Complex Vj = std::polar(buses[j].Vm, buses[j].Va);

        Complex Vi_tap = Vi / tapRatio;
        Complex Iij = (y + bShunt) * Vi_tap - y * Vj;
        Complex Iji = (y + bShunt) * Vj - y * Vi_tap;

        Complex Sij = Vi_tap * std::conj(Iij) * grid.baseMVA;
        Complex Sji = Vj * std::conj(Iji) * grid.baseMVA;

        flow.Pfrom = Sij.real();
        flow.Qfrom = Sij.imag();
        flow.Pto = Sji.real();
        flow.Qto = Sji.imag();
        flow.Ploss = flow.Pfrom + flow.Pto;
        flow.Qloss = flow.Qfrom + flow.Qto;

        solution.totalLossesP += flow.Ploss;
        solution.totalLossesQ += flow.Qloss;
        solution.branchFlows.push_back(flow);
    }
}

PowerFlowSolution NewtonRaphsonSolver::solve(
    const GridData& grid,
    const SolverConfig& config,
    CancellationToken* token,
    ProgressCallback progress
) {
    auto startTime = std::chrono::high_resolution_clock::now();

    PowerFlowSolution solution;
    solution.converged = false;
    solution.iterations = 0;
    solution.tolerance = config.tolerance;

    uint64_t requestId = 0;

    try {
        if (token) {
            token->throwIfCancelled();
        }

        requestId = m_state.beginComputation();
        m_state.resetCancellationToken(config.timeoutMs);

        {
            ComputationGuard guard(m_state.resultMutex(), m_state.cancellationToken());

            if (token) {
                token->throwIfCancelled();
            }

            std::vector<Bus> buses = grid.buses;
            std::vector<std::vector<Complex>> Ybus;
            buildYbus(grid, Ybus);

            if (token) token->throwIfCancelled();

            int n = grid.nBus();
            std::vector<int> nonSlackIdx;
            std::vector<int> pqIdx;

            for (int k = 0; k < n; k++) {
                if (buses[k].type != BusType::SLACK) {
                    nonSlackIdx.push_back(k);
                }
                if (buses[k].type == BusType::PQ) {
                    pqIdx.push_back(k);
                }
            }

            int nNonSlack = static_cast<int>(nonSlackIdx.size());
            int nPQ = static_cast<int>(pqIdx.size());

            for (int iter = 0; iter < config.maxIterations; iter++) {
                if (token) token->throwIfCancelled();
                if (m_state.isStaleRequest(requestId)) {
                    solution.errorMessage = "Superseded by newer request";
                    solution.converged = false;
                    break;
                }

                solution.iterations = iter + 1;

                Matrix J;
                Vector mismatch;
                buildJacobian(grid, buses, Ybus, J, mismatch);

                if (J.empty() || mismatch.empty()) {
                    solution.converged = true;
                    break;
                }

                double maxMismatch = 0.0;
                for (double m : mismatch) {
                    maxMismatch = std::max(maxMismatch, std::abs(m));
                }

                auto iterTime = std::chrono::high_resolution_clock::now();
                double iterElapsed = std::chrono::duration<double, std::milli>(iterTime - startTime).count();
                reportProgress(progress, iter + 1, config.maxIterations, maxMismatch, config.tolerance, iterElapsed);

                if (maxMismatch < config.tolerance) {
                    solution.converged = true;
                    break;
                }

                Vector dx;
                if (!solveLinearSystem(J, mismatch, dx)) {
                    solution.errorMessage = "Singular Jacobian matrix at iteration " + std::to_string(iter + 1);
                    break;
                }

                for (int k = 0; k < nNonSlack; k++) {
                    buses[nonSlackIdx[k]].Va += dx[k];
                }
                for (int k = 0; k < nPQ; k++) {
                    buses[pqIdx[k]].Vm += dx[nNonSlack + k];
                }

                for (auto& bus : buses) {
                    bus.Vm = std::max(bus.VmMin, std::min(bus.VmMax, bus.Vm));
                }
            }

            if (token) token->throwIfCancelled();

            solution.buses = buses;
            solution.branches = grid.branches;
            solution.generators = grid.generators;

            for (auto& bus : solution.buses) {
                bus.Pg = 0;
                bus.Qg = 0;
            }

            for (const auto& gen : grid.generators) {
                for (auto& bus : solution.buses) {
                    if (bus.id == gen.busId) {
                        bus.Pg = gen.Pg;
                        break;
                    }
                }
            }

            for (size_t k = 0; k < solution.buses.size(); k++) {
                if (solution.buses[k].type == BusType::SLACK || solution.buses[k].type == BusType::PV) {
                    double Vk = solution.buses[k].Vm;
                    double thetak = solution.buses[k].Va;
                    double Pcalc = 0.0, Qcalc = 0.0;
                    for (size_t m = 0; m < solution.buses.size(); m++) {
                        double Vm = solution.buses[m].Vm;
                        double theta_m = solution.buses[m].Va;
                        double dTheta = thetak - theta_m;
                        double Gkm = Ybus[k][m].real();
                        double Bkm = Ybus[k][m].imag();
                        Pcalc += Vk * Vm * (Gkm * cos(dTheta) + Bkm * sin(dTheta));
                        Qcalc += Vk * Vm * (Gkm * sin(dTheta) - Bkm * cos(dTheta));
                    }
                    if (solution.buses[k].type == BusType::SLACK) {
                        solution.buses[k].Pg = Pcalc * grid.baseMVA + solution.buses[k].Pd;
                    }
                    solution.buses[k].Qg = Qcalc * grid.baseMVA + solution.buses[k].Qd;
                }
            }

            calculateBranchFlows(grid, solution.buses, Ybus, solution);

        }

    } catch (const std::exception& e) {
        solution.errorMessage = e.what();
        solution.converged = false;
    }

    m_state.endComputation();

    auto endTime = std::chrono::high_resolution_clock::now();
    solution.elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return solution;
}

PowerFlowSolution NewtonRaphsonSolver::solveWithModifications(
    const GridData& grid,
    const SolverConfig& config,
    const std::vector<std::pair<int, double>>& genPowerChanges,
    const std::vector<std::pair<int, double>>& transformerTapChanges,
    CancellationToken* token,
    ProgressCallback progress
) {
    GridData modifiedGrid = grid;

    for (const auto& change : genPowerChanges) {
        int genId = change.first;
        double newPg = change.second;
        if (genId >= 0 && genId < static_cast<int>(modifiedGrid.generators.size())) {
            modifiedGrid.generators[genId].Pg = newPg;
            int busId = modifiedGrid.generators[genId].busId;
            for (auto& bus : modifiedGrid.buses) {
                if (bus.id == busId) {
                    bus.Pg = newPg;
                    break;
                }
            }
        }
    }

    for (const auto& change : transformerTapChanges) {
        int branchIdx = change.first;
        double newTap = change.second;
        if (branchIdx >= 0 && branchIdx < static_cast<int>(modifiedGrid.branches.size())) {
            modifiedGrid.branches[branchIdx].tap = newTap;
        }
    }

    return solve(modifiedGrid, config, token, progress);
}

} // namespace GridSolver
