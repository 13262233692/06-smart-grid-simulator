#include <napi.h>
#include "binding/GridSolverWrapper.h"

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    return GridSolver::Binding::GridSolverWrapper::Init(env, exports);
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitAll)
