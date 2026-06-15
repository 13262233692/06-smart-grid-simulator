#include "binding/GridSolverWrapper.h"
#include "core/CDFParser.h"
#include <memory>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GridSolver {
namespace Binding {

Napi::FunctionReference GridSolverWrapper::constructor;

Napi::Object GridSolverWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(env, "GridSolver", {
        InstanceMethod("loadCDF", &GridSolverWrapper::LoadCDF),
        InstanceMethod("solvePowerFlow", &GridSolverWrapper::SolvePowerFlow),
        InstanceMethod("solveWithModifications", &GridSolverWrapper::SolveWithModifications),
        InstanceMethod("getGridInfo", &GridSolverWrapper::GetGridInfo),
        InstanceMethod("isGridLoaded", &GridSolverWrapper::IsGridLoaded),
        InstanceMethod("cancelComputation", &GridSolverWrapper::CancelComputation),
        InstanceMethod("isComputing", &GridSolverWrapper::IsComputing),
        InstanceMethod("solvePowerFlowAsync", &GridSolverWrapper::SolvePowerFlowAsync),
        InstanceMethod("solveWithModificationsAsync", &GridSolverWrapper::SolveWithModificationsAsync)
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("GridSolver", func);
    return exports;
}

GridSolverWrapper::GridSolverWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<GridSolverWrapper>(info), m_gridLoaded(false),
      m_solverState(std::make_shared<SharedSolverState>()) {}

Napi::Value GridSolverWrapper::IsGridLoaded(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), m_gridLoaded);
}

Napi::Value GridSolverWrapper::IsComputing(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), m_solverState->computing.load());
}

Napi::Value GridSolverWrapper::CancelComputation(const Napi::CallbackInfo& info) {
    m_solverState->token.cancel();
    m_solverState->solver.state().cancelCurrent();
    return Napi::Boolean::New(info.Env(), true);
}

Napi::Value GridSolverWrapper::LoadCDF(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected file path string").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string filePath = info[0].As<Napi::String>().Utf8Value();

    try {
        CDFParser parser;
        m_gridData = parser.parse(filePath);
        m_gridLoaded = true;
        return GetGridInfo(info);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

SolverConfig parseConfig(const Napi::Value& val) {
    SolverConfig config;
    if (!val.IsObject()) return config;
    Napi::Object obj = val.As<Napi::Object>();
    if (obj.Has("tolerance") && obj.Get("tolerance").IsNumber()) {
        config.tolerance = obj.Get("tolerance").As<Napi::Number>().DoubleValue();
    }
    if (obj.Has("maxIterations") && obj.Get("maxIterations").IsNumber()) {
        config.maxIterations = obj.Get("maxIterations").As<Napi::Number>().Int32Value();
    }
    if (obj.Has("timeoutMs") && obj.Get("timeoutMs").IsNumber()) {
        config.timeoutMs = obj.Get("timeoutMs").As<Napi::Number>().DoubleValue();
    }
    return config;
}

std::vector<std::pair<int, double>> parseChanges(const Napi::Value& val) {
    std::vector<std::pair<int, double>> result;
    if (!val.IsArray()) return result;
    Napi::Array arr = val.As<Napi::Array>();
    for (uint32_t i = 0; i < arr.Length(); i++) {
        if (arr.Get(i).IsArray()) {
            Napi::Array item = arr.Get(i).As<Napi::Array>();
            if (item.Length() >= 2) {
                int id = item.Get((uint32_t)0).As<Napi::Number>().Int32Value();
                double val = item.Get((uint32_t)1).As<Napi::Number>().DoubleValue();
                result.push_back({id, val});
            }
        }
    }
    return result;
}

Napi::Value GridSolverWrapper::SolvePowerFlow(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_gridLoaded) {
        Napi::Error::New(env, "Grid data not loaded").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    SolverConfig config = parseConfig(info.Length() >= 1 ? info[0] : env.Undefined());

    try {
        CancellationToken token(config.timeoutMs);
        PowerFlowSolution sol = m_solverState->solver.solve(m_gridData, config, &token, nullptr);
        return solutionToObject(env, sol);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

Napi::Value GridSolverWrapper::SolveWithModifications(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_gridLoaded) {
        Napi::Error::New(env, "Grid data not loaded").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    SolverConfig config = parseConfig(info.Length() >= 1 ? info[0] : env.Undefined());
    auto genChanges = parseChanges(info.Length() >= 2 ? info[1] : env.Undefined());
    auto tapChanges = parseChanges(info.Length() >= 3 ? info[2] : env.Undefined());

    try {
        CancellationToken token(config.timeoutMs);
        PowerFlowSolution sol = m_solverState->solver.solveWithModifications(
            m_gridData, config, genChanges, tapChanges, &token, nullptr);
        return solutionToObject(env, sol);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

Napi::Value GridSolverWrapper::GetGridInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_gridLoaded) {
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("loaded", false);
        return obj;
    }
    return gridDataToObject(env, m_gridData);
}

class SolveAsyncWorker : public Napi::AsyncWorker {
public:
    SolveAsyncWorker(
        Napi::Function& callback,
        Napi::Promise::Deferred& deferred,
        std::shared_ptr<SharedSolverState> state,
        GridData grid,
        SolverConfig config,
        std::vector<std::pair<int, double>> genChanges,
        std::vector<std::pair<int, double>> tapChanges,
        Napi::ThreadSafeFunction progressTsfn
    ) : Napi::AsyncWorker(callback),
        m_deferred(deferred),
        m_state(state),
        m_grid(grid),
        m_config(config),
        m_genChanges(genChanges),
        m_tapChanges(tapChanges),
        m_progressTsfn(progressTsfn),
        m_requestId(0)
    {}

    void Execute() override {
        try {
            m_state->computing.store(true);
            m_requestId = ++m_state->activeRequestId;
            m_state->token = CancellationToken(m_config.timeoutMs);

            auto progressFn = [this](const IterationProgress& p) {
                if (m_progressTsfn) {
                    IterationProgress* pData = new IterationProgress(p);
                    m_progressTsfn.NonBlockingCall(pData, [](Napi::Env env, Napi::Function jsCallback, IterationProgress* data) {
                        Napi::Object obj = Napi::Object::New(env);
                        obj.Set("currentIteration", data->currentIteration);
                        obj.Set("maxIterations", data->maxIterations);
                        obj.Set("maxMismatch", data->maxMismatch);
                        obj.Set("tolerance", data->tolerance);
                        obj.Set("elapsedMs", data->elapsedMs);
                        jsCallback.Call({obj});
                        delete data;
                    });
                }
            };

            m_solution = m_state->solver.solveWithModifications(
                m_grid, m_config, m_genChanges, m_tapChanges,
                &m_state->token, progressFn);

            if (m_requestId < m_state->activeRequestId.load()) {
                m_solution.converged = false;
                m_solution.errorMessage = "Superseded by newer computation request";
            }
        } catch (const std::exception& e) {
            m_errorMsg = e.what();
            m_solution.converged = false;
            m_solution.errorMessage = e.what();
        }
        m_state->computing.store(false);
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::HandleScope scope(env);

        if (m_progressTsfn) {
            m_progressTsfn.Release();
        }

        if (!m_errorMsg.empty() && !m_solution.converged && m_solution.buses.empty()) {
            m_deferred.Reject(Napi::Error::New(env, m_errorMsg).Value());
        } else {
            Napi::Object result = Napi::Object::New(env);
            result.Set("success", Napi::Boolean::New(env, m_solution.converged || !m_solution.buses.empty()));
            result.Set("data", solutionToJsObject(env, m_solution));
            m_deferred.Resolve(result);
        }
    }

    void OnError(const Napi::Error& error) override {
        if (m_progressTsfn) {
            m_progressTsfn.Release();
        }
        m_deferred.Reject(error.Value());
    }

private:
    Napi::Promise::Deferred m_deferred;
    std::shared_ptr<SharedSolverState> m_state;
    GridData m_grid;
    SolverConfig m_config;
    std::vector<std::pair<int, double>> m_genChanges;
    std::vector<std::pair<int, double>> m_tapChanges;
    Napi::ThreadSafeFunction m_progressTsfn;
    PowerFlowSolution m_solution;
    std::string m_errorMsg;
    uint64_t m_requestId;

    Napi::Object solutionToJsObject(Napi::Env env, const PowerFlowSolution& sol) {
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("converged", sol.converged);
        obj.Set("iterations", sol.iterations);
        obj.Set("tolerance", sol.tolerance);
        obj.Set("elapsedMs", sol.elapsedMs);
        obj.Set("errorMessage", sol.errorMessage);

        Napi::Array busArr = Napi::Array::New(env, sol.buses.size());
        for (size_t i = 0; i < sol.buses.size(); i++) {
            const auto& b = sol.buses[i];
            Napi::Object bo = Napi::Object::New(env);
            bo.Set("id", b.id); bo.Set("name", b.name);
            bo.Set("type", static_cast<int>(b.type));
            bo.Set("typeName", b.type == BusType::SLACK ? "SLACK" : (b.type == BusType::PV ? "PV" : "PQ"));
            bo.Set("baseKV", b.baseKV); bo.Set("Vm", b.Vm);
            bo.Set("Va", b.Va * 180.0 / M_PI);
            bo.Set("VmMax", b.VmMax); bo.Set("VmMin", b.VmMin);
            bo.Set("Pd", b.Pd); bo.Set("Qd", b.Qd);
            bo.Set("Pg", b.Pg); bo.Set("Qg", b.Qg);
            bo.Set("QgMax", b.QgMax); bo.Set("QgMin", b.QgMin);
            bo.Set("shuntG", b.shuntG); bo.Set("shuntB", b.shuntB);
            bo.Set("area", b.area); bo.Set("zone", b.zone);
            bo.Set("x", b.x); bo.Set("y", b.y);
            busArr.Set(static_cast<uint32_t>(i), bo);
        }
        obj.Set("buses", busArr);

        Napi::Array brArr = Napi::Array::New(env, sol.branches.size());
        for (size_t i = 0; i < sol.branches.size(); i++) {
            const auto& br = sol.branches[i];
            Napi::Object bro = Napi::Object::New(env);
            bro.Set("id", br.id); bro.Set("fromBus", br.fromBus); bro.Set("toBus", br.toBus);
            bro.Set("r", br.r); bro.Set("x", br.x); bro.Set("b", br.b);
            bro.Set("tap", br.tap); bro.Set("phaseShift", br.phaseShift * 180.0 / M_PI);
            bro.Set("status", br.status);
            bro.Set("rateA", br.rateA); bro.Set("rateB", br.rateB); bro.Set("rateC", br.rateC);
            bro.Set("ratioMin", br.ratioMin); bro.Set("ratioMax", br.ratioMax);
            bro.Set("isTransformer", br.tap != 1.0 || br.phaseShift != 0);
            brArr.Set(static_cast<uint32_t>(i), bro);
        }
        obj.Set("branches", brArr);

        Napi::Array genArr = Napi::Array::New(env, sol.generators.size());
        for (size_t i = 0; i < sol.generators.size(); i++) {
            const auto& g = sol.generators[i];
            Napi::Object go = Napi::Object::New(env);
            go.Set("id", g.id); go.Set("busId", g.busId);
            go.Set("Pg", g.Pg); go.Set("Qg", g.Qg);
            go.Set("PgMax", g.PgMax); go.Set("PgMin", g.PgMin);
            go.Set("QgMax", g.QgMax); go.Set("QgMin", g.QgMin);
            go.Set("Vset", g.Vset); go.Set("status", g.status);
            genArr.Set(static_cast<uint32_t>(i), go);
        }
        obj.Set("generators", genArr);

        Napi::Array flArr = Napi::Array::New(env, sol.branchFlows.size());
        for (size_t i = 0; i < sol.branchFlows.size(); i++) {
            const auto& f = sol.branchFlows[i];
            Napi::Object fo = Napi::Object::New(env);
            fo.Set("fromBus", f.fromBus); fo.Set("toBus", f.toBus);
            fo.Set("Pfrom", f.Pfrom); fo.Set("Qfrom", f.Qfrom);
            fo.Set("Pto", f.Pto); fo.Set("Qto", f.Qto);
            fo.Set("Ploss", f.Ploss); fo.Set("Qloss", f.Qloss);
            flArr.Set(static_cast<uint32_t>(i), fo);
        }
        obj.Set("branchFlows", flArr);
        obj.Set("totalLossesP", sol.totalLossesP);
        obj.Set("totalLossesQ", sol.totalLossesQ);
        return obj;
    }
};

Napi::Value GridSolverWrapper::SolvePowerFlowAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!m_gridLoaded) {
        Napi::Error::New(env, "Grid data not loaded").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    SolverConfig config = parseConfig(info.Length() >= 1 ? info[0] : env.Undefined());

    Napi::Function callback = info.Length() >= 2 && info[1].IsFunction()
        ? info[1].As<Napi::Function>()
        : Napi::Function::New(env, [](const Napi::CallbackInfo&) {});

    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

    Napi::ThreadSafeFunction progressTsfn;
    if (info.Length() >= 3 && info[2].IsFunction()) {
        progressTsfn = Napi::ThreadSafeFunction::New(
            env, info[2].As<Napi::Function>(), "ProgressCallback", 0, 1);
    }

    m_solverState->token.cancel();

    auto* worker = new SolveAsyncWorker(
        callback, deferred, m_solverState, m_gridData, config,
        {}, {}, progressTsfn);
    worker->Queue();

    return deferred.Promise();
}

Napi::Value GridSolverWrapper::SolveWithModificationsAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!m_gridLoaded) {
        Napi::Error::New(env, "Grid data not loaded").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    SolverConfig config = parseConfig(info.Length() >= 1 ? info[0] : env.Undefined());
    auto genChanges = parseChanges(info.Length() >= 2 ? info[1] : env.Undefined());
    auto tapChanges = parseChanges(info.Length() >= 3 ? info[2] : env.Undefined());

    Napi::Function callback = info.Length() >= 4 && info[3].IsFunction()
        ? info[3].As<Napi::Function>()
        : Napi::Function::New(env, [](const Napi::CallbackInfo&) {});

    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

    Napi::ThreadSafeFunction progressTsfn;
    if (info.Length() >= 5 && info[4].IsFunction()) {
        progressTsfn = Napi::ThreadSafeFunction::New(
            env, info[4].As<Napi::Function>(), "ProgressCallback", 0, 1);
    }

    m_solverState->token.cancel();

    auto* worker = new SolveAsyncWorker(
        callback, deferred, m_solverState, m_gridData, config,
        genChanges, tapChanges, progressTsfn);
    worker->Queue();

    return deferred.Promise();
}

Napi::Object GridSolverWrapper::gridDataToObject(Napi::Env env, const GridData& grid) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("loaded", true);
    obj.Set("caseName", Napi::String::New(env, grid.caseName));
    obj.Set("sourceFile", Napi::String::New(env, grid.sourceFile));
    obj.Set("baseMVA", Napi::Number::New(env, grid.baseMVA));
    obj.Set("nBus", Napi::Number::New(env, grid.nBus()));
    obj.Set("nBranch", Napi::Number::New(env, grid.nBranch()));
    obj.Set("nGen", Napi::Number::New(env, grid.nGen()));
    obj.Set("nPQ", Napi::Number::New(env, grid.nPQ()));
    obj.Set("nPV", Napi::Number::New(env, grid.nPV()));
    obj.Set("nSlack", Napi::Number::New(env, grid.nSlack()));
    obj.Set("buses", busesToArray(env, grid.buses));
    obj.Set("branches", branchesToArray(env, grid.branches));
    obj.Set("generators", generatorsToArray(env, grid.generators));
    return obj;
}

Napi::Array GridSolverWrapper::busesToArray(Napi::Env env, const std::vector<Bus>& buses) {
    Napi::Array arr = Napi::Array::New(env, buses.size());
    for (size_t i = 0; i < buses.size(); i++) {
        const auto& b = buses[i];
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("id", Napi::Number::New(env, b.id));
        obj.Set("name", Napi::String::New(env, b.name));
        obj.Set("type", Napi::Number::New(env, static_cast<int>(b.type)));
        obj.Set("typeName", Napi::String::New(env,
            b.type == BusType::SLACK ? "SLACK" : (b.type == BusType::PV ? "PV" : "PQ")));
        obj.Set("baseKV", Napi::Number::New(env, b.baseKV));
        obj.Set("Vm", Napi::Number::New(env, b.Vm));
        obj.Set("Va", Napi::Number::New(env, b.Va * 180.0 / M_PI));
        obj.Set("VmMax", Napi::Number::New(env, b.VmMax));
        obj.Set("VmMin", Napi::Number::New(env, b.VmMin));
        obj.Set("Pd", Napi::Number::New(env, b.Pd));
        obj.Set("Qd", Napi::Number::New(env, b.Qd));
        obj.Set("Pg", Napi::Number::New(env, b.Pg));
        obj.Set("Qg", Napi::Number::New(env, b.Qg));
        obj.Set("QgMax", Napi::Number::New(env, b.QgMax));
        obj.Set("QgMin", Napi::Number::New(env, b.QgMin));
        obj.Set("shuntG", Napi::Number::New(env, b.shuntG));
        obj.Set("shuntB", Napi::Number::New(env, b.shuntB));
        obj.Set("area", Napi::Number::New(env, b.area));
        obj.Set("zone", Napi::Number::New(env, b.zone));
        obj.Set("x", Napi::Number::New(env, b.x));
        obj.Set("y", Napi::Number::New(env, b.y));
        arr.Set(static_cast<uint32_t>(i), obj);
    }
    return arr;
}

Napi::Array GridSolverWrapper::branchesToArray(Napi::Env env, const std::vector<Branch>& branches) {
    Napi::Array arr = Napi::Array::New(env, branches.size());
    for (size_t i = 0; i < branches.size(); i++) {
        const auto& br = branches[i];
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("id", Napi::Number::New(env, br.id));
        obj.Set("fromBus", Napi::Number::New(env, br.fromBus));
        obj.Set("toBus", Napi::Number::New(env, br.toBus));
        obj.Set("r", Napi::Number::New(env, br.r));
        obj.Set("x", Napi::Number::New(env, br.x));
        obj.Set("b", Napi::Number::New(env, br.b));
        obj.Set("tap", Napi::Number::New(env, br.tap));
        obj.Set("phaseShift", Napi::Number::New(env, br.phaseShift * 180.0 / M_PI));
        obj.Set("status", Napi::Number::New(env, br.status));
        obj.Set("rateA", Napi::Number::New(env, br.rateA));
        obj.Set("rateB", Napi::Number::New(env, br.rateB));
        obj.Set("rateC", Napi::Number::New(env, br.rateC));
        obj.Set("ratioMin", Napi::Number::New(env, br.ratioMin));
        obj.Set("ratioMax", Napi::Number::New(env, br.ratioMax));
        obj.Set("isTransformer", Napi::Boolean::New(env, br.tap != 1.0 || br.phaseShift != 0));
        arr.Set(static_cast<uint32_t>(i), obj);
    }
    return arr;
}

Napi::Array GridSolverWrapper::generatorsToArray(Napi::Env env, const std::vector<Generator>& generators) {
    Napi::Array arr = Napi::Array::New(env, generators.size());
    for (size_t i = 0; i < generators.size(); i++) {
        const auto& g = generators[i];
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("id", Napi::Number::New(env, g.id));
        obj.Set("busId", Napi::Number::New(env, g.busId));
        obj.Set("Pg", Napi::Number::New(env, g.Pg));
        obj.Set("Qg", Napi::Number::New(env, g.Qg));
        obj.Set("PgMax", Napi::Number::New(env, g.PgMax));
        obj.Set("PgMin", Napi::Number::New(env, g.PgMin));
        obj.Set("QgMax", Napi::Number::New(env, g.QgMax));
        obj.Set("QgMin", Napi::Number::New(env, g.QgMin));
        obj.Set("Vset", Napi::Number::New(env, g.Vset));
        obj.Set("status", Napi::Number::New(env, g.status));
        arr.Set(static_cast<uint32_t>(i), obj);
    }
    return arr;
}

Napi::Array GridSolverWrapper::flowsToArray(Napi::Env env, const std::vector<FlowResult>& flows) {
    Napi::Array arr = Napi::Array::New(env, flows.size());
    for (size_t i = 0; i < flows.size(); i++) {
        const auto& f = flows[i];
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("fromBus", Napi::Number::New(env, f.fromBus));
        obj.Set("toBus", Napi::Number::New(env, f.toBus));
        obj.Set("Pfrom", Napi::Number::New(env, f.Pfrom));
        obj.Set("Qfrom", Napi::Number::New(env, f.Qfrom));
        obj.Set("Pto", Napi::Number::New(env, f.Pto));
        obj.Set("Qto", Napi::Number::New(env, f.Qto));
        obj.Set("Ploss", Napi::Number::New(env, f.Ploss));
        obj.Set("Qloss", Napi::Number::New(env, f.Qloss));
        arr.Set(static_cast<uint32_t>(i), obj);
    }
    return arr;
}

Napi::Object GridSolverWrapper::solutionToObject(Napi::Env env, const PowerFlowSolution& sol) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("converged", Napi::Boolean::New(env, sol.converged));
    obj.Set("iterations", Napi::Number::New(env, sol.iterations));
    obj.Set("tolerance", Napi::Number::New(env, sol.tolerance));
    obj.Set("elapsedMs", Napi::Number::New(env, sol.elapsedMs));
    obj.Set("errorMessage", Napi::String::New(env, sol.errorMessage));
    obj.Set("buses", busesToArray(env, sol.buses));
    obj.Set("branches", branchesToArray(env, sol.branches));
    obj.Set("generators", generatorsToArray(env, sol.generators));
    obj.Set("branchFlows", flowsToArray(env, sol.branchFlows));
    obj.Set("totalLossesP", Napi::Number::New(env, sol.totalLossesP));
    obj.Set("totalLossesQ", Napi::Number::New(env, sol.totalLossesQ));
    return obj;
}

} // namespace Binding
} // namespace GridSolver
