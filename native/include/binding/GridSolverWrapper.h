#pragma once
#include <napi.h>
#include "core/GridData.h"
#include "solver/NewtonRaphsonSolver.h"
#include <mutex>
#include <memory>

namespace GridSolver {
namespace Binding {

struct SharedSolverState {
    std::mutex mutex;
    NewtonRaphsonSolver solver;
    CancellationToken token;
    std::atomic<uint64_t> activeRequestId{0};
    std::atomic<bool> computing{false};
};

class GridSolverWrapper : public Napi::ObjectWrap<GridSolverWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    GridSolverWrapper(const Napi::CallbackInfo& info);

private:
    static Napi::FunctionReference constructor;

    GridData m_gridData;
    bool m_gridLoaded;
    std::shared_ptr<SharedSolverState> m_solverState;

    Napi::Value LoadCDF(const Napi::CallbackInfo& info);
    Napi::Value SolvePowerFlow(const Napi::CallbackInfo& info);
    Napi::Value SolveWithModifications(const Napi::CallbackInfo& info);
    Napi::Value GetGridInfo(const Napi::CallbackInfo& info);
    Napi::Value IsGridLoaded(const Napi::CallbackInfo& info);
    Napi::Value CancelComputation(const Napi::CallbackInfo& info);
    Napi::Value IsComputing(const Napi::CallbackInfo& info);

    Napi::Value SolvePowerFlowAsync(const Napi::CallbackInfo& info);
    Napi::Value SolveWithModificationsAsync(const Napi::CallbackInfo& info);

    Napi::Object gridDataToObject(Napi::Env env, const GridData& grid);
    Napi::Object solutionToObject(Napi::Env env, const PowerFlowSolution& sol);
    Napi::Array busesToArray(Napi::Env env, const std::vector<Bus>& buses);
    Napi::Array branchesToArray(Napi::Env env, const std::vector<Branch>& branches);
    Napi::Array generatorsToArray(Napi::Env env, const std::vector<Generator>& generators);
    Napi::Array flowsToArray(Napi::Env env, const std::vector<FlowResult>& flows);
};

} // namespace Binding
} // namespace GridSolver
