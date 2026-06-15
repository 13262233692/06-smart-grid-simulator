# Smart Grid Simulator - 电网拓扑与潮流计算系统

面向电力调度中心的专业级电网潮流计算桌面应用，采用 C++ + Node-API + Electron + Vue3 技术栈。

## 技术架构

```
┌─────────────────────────────────────────────────────────────┐
│                     Electron 桌面应用                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Vue3 前端 (渲染进程)                                   │  │
│  │  ├─ Canvas 电网单线图渲染引擎                           │  │
│  │  ├─ 控制面板（发电机/变压器调节）                       │  │
│  │  └─ 数据面板（节点/支路详情）                           │  │
│  └───────────────────────┬───────────────────────────────┘  │
│                          │ IPC                               │
│  ┌───────────────────────▼───────────────────────────────┐  │
│  │  Electron 主进程                                        │  │
│  └───────────────────────┬───────────────────────────────┘  │
│                          │ N-API                             │
│  ┌───────────────────────▼───────────────────────────────┐  │
│  │  C++ 计算引擎 (原生模块)                                │  │
│  │  ├─ IEEE CDF 解析器                                    │  │
│  │  ├─ 牛顿-拉夫逊潮流求解器                               │  │
│  │  │   ├─ 节点导纳矩阵构建                                │  │
│  │  │   ├─ 雅可比矩阵构造                                  │  │
│  │  │   └─ LU 分解线性求解                                 │  │
│  │  └─ 支路潮流计算                                       │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## 项目目录结构

```
06-smart-grid-simulator/
├── native/                      # C++ 原生计算模块
│   ├── CMakeLists.txt           # CMake 构建配置
│   ├── package.json             # cmake-js 配置
│   ├── index.js                 # JS 封装入口
│   ├── include/                 # 头文件
│   │   ├── core/                # 核心数据结构
│   │   │   ├── GridData.h       # 电网数据模型
│   │   │   └── CDFParser.h      # CDF解析器
│   │   ├── solver/              # 求解器
│   │   │   └── NewtonRaphsonSolver.h
│   │   └── binding/             # N-API 绑定
│   │       └── GridSolverWrapper.h
│   └── src/                     # 源代码
│       ├── core/
│       │   ├── GridData.cpp
│       │   └── CDFParser.cpp
│       ├── solver/
│       │   └── NewtonRaphsonSolver.cpp
│       └── binding/
│           ├── GridSolverWrapper.cpp
│           └── main.cpp         # N-API 模块入口
│
├── frontend/                    # Electron + Vue3 前端
│   ├── package.json
│   ├── vite.config.js
│   ├── index.html
│   ├── electron/
│   │   ├── main/                # Electron 主进程
│   │   └── preload/             # 预加载脚本
│   └── src/
│       ├── main.js              # Vue 入口
│       ├── App.vue              # 主应用
│       ├── style.css            # 全局样式
│       ├── stores/
│       │   └── grid.js          # Pinia 状态管理
│       └── components/
│           ├── Toolbar.vue      # 顶部工具栏
│           ├── GridCanvas.vue   # Canvas 电网渲染
│           ├── ControlPanel.vue # 控制面板
│           └── DataPanel.vue    # 数据面板
│
├── samples/                     # 标准算例
│   ├── ieee14.cdf               # IEEE 14 节点
│   └── ieee5.cdf                # 简化 5 节点
│
└── test/
    └── run-tests.js             # 项目验证脚本
```

## 核心功能

### 1. C++ 底层计算引擎

- **IEEE CDF 文件解析器**
  - 支持标准 IEEE Common Data Format 格式
  - 解析母线数据、支路数据、变压器参数
  - 自动构建节点索引映射

- **牛顿-拉夫逊法潮流求解**
  - 节点导纳矩阵 (Ybus) 构建
  - 完整雅可比矩阵 (Jacobian) 构造
    - H 子矩阵: dP/dθ
    - N 子矩阵: dP/dV
    - M 子矩阵: dQ/dθ
    - L 子矩阵: dQ/dV
  - LU 分解带选主元求解线性方程组
  - 迭代收敛检测（最大不匹配量判定）
  - 电压越限自动处理

- **支路潮流计算**
  - 支持带分接头变压器模型
  - 双端功率计算与网损统计
  - 线路充电功率考虑

### 2. N-API 桥接层

- 同步/异步双模式调用接口
- AsyncWorker 异步工作线程
- C++ 复杂对象 ↔ JavaScript 对象自动转换
- 错误捕获与异常传递
- 支持动态参数修改（发电机出力、变压器变比）

### 3. Electron + Vue3 前端界面

- **Canvas 电网单线图**
  - SLACK (绿色) / PV (橙色) / PQ (蓝色) 节点类型区分
  - 变压器双圆圈特殊符号
  - 潮流方向动画箭头
  - 线路粗细随潮流大小变化
  - 节点电压幅值色阶显示
  - 鼠标缩放/平移/拖拽节点/点选交互

- **控制面板**
  - 求解器参数配置（容差、迭代次数）
  - 发电机有功出力滑块调节
  - 变压器变比滑块调节
  - 批量修改一键应用重新计算

- **数据面板**
  - 节点/支路详细数据查看
  - 系统运行概览统计
  - 求解结果汇总（收敛状态、迭代次数、网损）
  - 母线表 / 支路表完整数据浏览

## 构建与运行

### 环境要求

- Node.js >= 18.x
- CMake >= 3.15
- C++17 兼容编译器 (MSVC 2019+ / GCC 9+ / Clang 10+)
- Python 3.x (node-gyp 依赖)

### Windows 构建步骤

```powershell
# 1. 安装根目录依赖
cd d:\SOLO-12\06-smart-grid-simulator
npm install

# 2. 构建 C++ 原生模块
npm run build:native

# 3. 安装前端依赖
cd frontend
npm install
cd ..

# 4. 启动开发模式 (Electron + Vite HMR)
npm run dev

# 5. 或生产构建
npm run build
```

### 项目验证

```powershell
# 运行项目结构验证
node test/run-tests.js
```

## API 接口示例

### JavaScript (N-API 模块)

```javascript
const { GridSolver } = require('./native');

const solver = new GridSolver();

// 异步加载 IEEE CDF 文件
const gridInfo = await solver.loadCDF('samples/ieee14.cdf');
console.log(`节点数: ${gridInfo.nBus}, 支路数: ${gridInfo.nBranch}`);

// 潮流计算
const solution = await solver.solvePowerFlow({
  tolerance: 1e-6,
  maxIterations: 50
});

if (solution.converged) {
  console.log(`迭代 ${solution.iterations} 次收敛`);
  console.log(`有功网损: ${solution.totalLossesP.toFixed(2)} MW`);
  solution.buses.forEach(bus => {
    console.log(`节点 ${bus.id}: V=${bus.Vm.toFixed(4)} pu, θ=${bus.Va.toFixed(2)}°`);
  });
}

// 动态修改发电机出力并重新计算
const modified = await solver.solveWithModifications(
  {}, // 配置
  [[0, 180.0]], // 修改 0 号发电机 Pg=180 MW
  [[3, 1.02]]   // 修改 3 号支路变比=1.02
);
```

## 标准算例

| 算例文件 | 节点数 | 支路数 | 发电机数 | 说明 |
|---------|--------|--------|---------|------|
| ieee5.cdf | 5 | 7 | 2 | 简化教学算例 |
| ieee14.cdf | 14 | 20 | 5 | IEEE 标准 14 节点系统 |

## 数学原理

### 牛顿-拉夫逊法潮流计算

极坐标下的功率平衡方程：

```
ΔPi = Pi_sched - Σⱼ Vi Vj (Gij cosθij + Bij sinθij)
ΔQi = Qi_sched - Σⱼ Vi Vj (Gij sinθij - Bij cosθij)
```

雅可比矩阵元素（非对角元素 i≠j）：

```
∂Pi/∂θj = Vi Vj (Gij sinθij - Bij cosθij)
∂Pi/∂Vj = Vi (Gij cosθij + Bij sinθij)
∂Qi/∂θj = -Vi Vj (Gij cosθij + Bij sinθij)
∂Qi/∂Vj = Vi (Gij sinθij - Bij cosθij)
```

每一步迭代求解修正方程 `J·Δx = -Δ`，更新状态变量直到最大不匹配量小于容差。

## License

MIT
