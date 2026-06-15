#include "binding/GridSolverWrapper.h"
#include "core/CDFParser.h"
#include <memory>

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
        InstanceMethod("loadCDFAsync", &GridSolverWrapper::LoadCDFAsync),
        InstanceMethod("solvePowerFlowAsync", &GridSolverWrapper::SolvePowerFlowAsync),
        InstanceMethod("solveWithModificationsAsync", &GridSolverWrapper::SolveWithModificationsAsync)
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("GridSolver", func);
    return exports;
}

GridSolverWrapper::GridSolverWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<GridSolverWrapper>(info), m_gridLoaded(false) {}

Napi::Value GridSolverWrapper::IsGridLoaded(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), m_gridLoaded);
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

Napi::Value GridSolverWrapper::SolvePowerFlow(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!m_gridLoaded) {
        Napi::Error::New(env, "Grid data not loaded").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    SolverConfig config;
    if (info.Length() >= 1 && info[0].IsObject()) {
        Napi::Object obj = info[0].As<Napi::Object>();
        if (obj.Has("tolerance") && obj.Get("tolerance").IsNumber()) {
            config.tolerance = obj.Get("tolerance").As<Napi::Number>().DoubleValue();
        }
        if (obj.Has("maxIterations") && obj.Get("maxIterations").IsNumber()) {
            config.maxIterations = obj.Get("maxIterations").As<Napi::Number>().Int32Value();
        }
    }

    try {
        NewtonRaphsonSolver solver;
        PowerFlowSolution sol = solver.solve(m_gridData, config);
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

    if (info.Length() < 3) {
        Napi::TypeError::New(env, "Expected 3 arguments: config, genChanges, tapChanges").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    SolverConfig config;
    if (info[0].IsObject()) {
        Napi::Object obj = info[0].As<Napi::Object>();
        if (obj.Has("tolerance") && obj.Get("tolerance").IsNumber()) {
            config.tolerance = obj.Get("tolerance").As<Napi::Number>().DoubleValue();
        }
        if (obj.Has("maxIterations") && obj.Get("maxIterations").IsNumber()) {
            config.maxIterations = obj.Get("maxIterations").As<Napi::Number>().Int32Value();
        }
    }

    std::vector<std::pair<int, double>> genChanges;
    if (info[1].IsArray()) {
        Napi::Array arr = info[1].As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); i++) {
            if (arr.Get(i).IsArray()) {
                Napi::Array item = arr.Get(i).As<Napi::Array>();
                if (item.Length() >= 2) {
                    int id = item.Get((uint32_t)0).As<Napi::Number>().Int32Value();
                    double pg = item.Get((uint32_t)1).As<Napi::Number>().DoubleValue();
                    genChanges.push_back({id, pg});
                }
            }
        }
    }

    std::vector<std::pair<int, double>> tapChanges;
    if (info[2].IsArray()) {
        Napi::Array arr = info[2].As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); i++) {
            if (arr.Get(i).IsArray()) {
                Napi::Array item = arr.Get(i).As<Napi::Array>();
                if (item.Length() >= 2) {
                    int id = item.Get((uint32_t)0).As<Napi::Number>().Int32Value();
                    double tap = item.Get((uint32_t)1).As<Napi::Number>().DoubleValue();
                    tapChanges.push_back({id, tap});
                }
            }
        }
    }

    try {
        NewtonRaphsonSolver solver;
        PowerFlowSolution sol = solver.solveWithModifications(m_gridData, config, genChanges, tapChanges);
        return solutionToObject(env, sol);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

Napi::Value GridSolverWrapper::GetGridInfo(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Object obj = Napi::Object::New(env);

    if (!m_gridLoaded) {
        obj.Set("loaded", false);
        return obj;
    }

    return gridDataToObject(env, m_gridData);
}

Napi::Value GridSolverWrapper::LoadCDFAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected (filePath, callback)").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string filePath = info[0].As<Napi::String>().Utf8Value();
    Napi::Function callback = info[1].As<Napi::Function>();

    auto* worker = new Napi::AsyncWorker(callback) {
    public:
        Napi::Promise::Deferred deferred;
        bool usePromise = false;
        std::string path;
        GridData grid;
        std::string errorMsg;
        bool success = false;
        GridSolverWrapper* wrapper;

        void Execute() override {
            try {
                CDFParser parser;
                grid = parser.parse(path);
                success = true;
            } catch (const std::exception& e) {
                errorMsg = e.what();
                success = false;
            }
        }

        void OnOK() override {
            Napi::Env env = Env();
            Napi::HandleScope scope(env);
            if (success) {
                wrapper->m_gridData = grid;
                wrapper->m_gridLoaded = true;
                Napi::Object result = wrapper->gridDataToObject(env, grid);
                if (usePromise) {
                    deferred.Resolve(result);
                } else {
                    Callback().MakeCallback(Receiver().Value(), {env.Null(), result});
                }
            } else {
                Napi::Error err = Napi::Error::New(env, errorMsg);
                if (usePromise) {
                    deferred.Reject(err.Value());
                } else {
                    Callback().MakeCallback(Receiver().Value(), {err.Value(), env.Undefined()});
                }
            }
        }
    };

    worker->wrapper = this;
    worker->path = filePath;
    worker->Queue();

    return env.Undefined();
}

Napi::Value GridSolverWrapper::SolvePowerFlowAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    SolverConfig config;
    Napi::Function callback;

    int cbIdx = 0;
    if (info.Length() >= 1 && info[0].IsObject() && !info[0].IsFunction()) {
        Napi::Object obj = info[0].As<Napi::Object>();
        if (obj.Has("tolerance") && obj.Get("tolerance").IsNumber()) {
            config.tolerance = obj.Get("tolerance").As<Napi::Number>().DoubleValue();
        }
        if (obj.Has("maxIterations") && obj.Get("maxIterations").IsNumber()) {
            config.maxIterations = obj.Get("maxIterations").As<Napi::Number>().Int32Value();
        }
        cbIdx = 1;
    }

    if (info.Length() < cbIdx + 1 || !info[cbIdx].IsFunction()) {
        Napi::TypeError::New(env, "Expected callback function").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    callback = info[cbIdx].As<Napi::Function>();

    if (!m_gridLoaded) {
        Napi::Error::New(env, "Grid data not loaded").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto* worker = new Napi::AsyncWorker(callback) {
    public:
        SolverConfig cfg;
        GridData grid;
        PowerFlowSolution solution;
        GridSolverWrapper* wrapper;
        std::string errorMsg;

        void Execute() override {
            try {
                NewtonRaphsonSolver solver;
                solution = solver.solve(grid, cfg);
            } catch (const std::exception& e) {
                errorMsg = e.what();
                solution.converged = false;
            }
        }

        void OnOK() override {
            Napi::Env env = Env();
            Napi::HandleScope scope(env);
            if (!errorMsg.empty() && !solution.converged) {
                Napi::Error err = Napi::Error::New(env, errorMsg);
                Callback().MakeCallback(Receiver().Value(), {err.Value(), env.Undefined()});
            } else {
                Napi::Object result = wrapper->solutionToObject(env, solution);
                Callback().MakeCallback(Receiver().Value(), {env.Null(), result});
            }
        }
    };

    worker->wrapper = this;
    worker->cfg = config;
    worker->grid = m_gridData;
    worker->Queue();

    return env.Undefined();
}

Napi::Value GridSolverWrapper::SolveWithModificationsAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4) {
        Napi::TypeError::New(env, "Expected (config, genChanges, tapChanges, callback)").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    SolverConfig config;
    if (info[0].IsObject()) {
        Napi::Object obj = info[0].As<Napi::Object>();
        if (obj.Has("tolerance") && obj.Get("tolerance").IsNumber()) {
            config.tolerance = obj.Get("tolerance").As<Napi::Number>().DoubleValue();
        }
        if (obj.Has("maxIterations") && obj.Get("maxIterations").IsNumber()) {
            config.maxIterations = obj.Get("maxIterations").As<Napi::Number>().Int32Value();
        }
    }

    std::vector<std::pair<int, double>> genChanges;
    if (info[1].IsArray()) {
        Napi::Array arr = info[1].As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); i++) {
            if (arr.Get(i).IsArray()) {
                Napi::Array item = arr.Get(i).As<Napi::Array>();
                if (item.Length() >= 2) {
                    int id = item.Get((uint32_t)0).As<Napi::Number>().Int32Value();
                    double pg = item.Get((uint32_t)1).As<Napi::Number>().DoubleValue();
                    genChanges.push_back({id, pg});
                }
            }
        }
    }

    std::vector<std::pair<int, double>> tapChanges;
    if (info[2].IsArray()) {
        Napi::Array arr = info[2].As<Napi::Array>();
        for (uint32_t i = 0; i < arr.Length(); i++) {
            if (arr.Get(i).IsArray()) {
                Napi::Array item = arr.Get(i).As<Napi::Array>();
                if (item.Length() >= 2) {
                    int id = item.Get((uint32_t)0).As<Napi::Number>().Int32Value();
                    double tap = item.Get((uint32_t)1).As<Napi::Number>().DoubleValue();
                    tapChanges.push_back({id, tap});
                }
            }
        }
    }

    if (!info[3].IsFunction()) {
        Napi::TypeError::New(env, "Expected callback function").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    Napi::Function callback = info[3].As<Napi::Function>();

    if (!m_gridLoaded) {
        Napi::Error::New(env, "Grid data not loaded").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto* worker = new Napi::AsyncWorker(callback) {
    public:
        SolverConfig cfg;
        GridData grid;
        std::vector<std::pair<int, double>> gc;
        std::vector<std::pair<int, double>> tc;
        PowerFlowSolution solution;
        GridSolverWrapper* wrapper;
        std::string errorMsg;

        void Execute() override {
            try {
                NewtonRaphsonSolver solver;
                solution = solver.solveWithModifications(grid, cfg, gc, tc);
            } catch (const std::exception& e) {
                errorMsg = e.what();
                solution.converged = false;
            }
        }

        void OnOK() override {
            Napi::Env env = Env();
            Napi::HandleScope scope(env);
            if (!errorMsg.empty() && !solution.converged) {
                Napi::Error err = Napi::Error::New(env, errorMsg);
                Callback().MakeCallback(Receiver().Value(), {err.Value(), env.Undefined()});
            } else {
                Napi::Object result = wrapper->solutionToObject(env, solution);
                Callback().MakeCallback(Receiver().Value(), {env.Null(), result});
            }
        }
    };

    worker->wrapper = this;
    worker->cfg = config;
    worker->grid = m_gridData;
    worker->gc = genChanges;
    worker->tc = tapChanges;
    worker->Queue();

    return env.Undefined();
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
