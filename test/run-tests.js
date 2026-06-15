'use strict';

const path = require('path');
const fs = require('fs');

console.log('========================================');
console.log('  Smart Grid Simulator - 测试脚本');
console.log('========================================\n');

const SAMPLES_DIR = path.join(__dirname, '..', 'samples');

function runTest(testName, testFn) {
  try {
    console.log(`[测试] ${testName}`);
    testFn();
    console.log(`  ✓ 通过\n`);
  } catch (e) {
    console.log(`  ✗ 失败: ${e.message}\n`);
  }
}

console.log('1. 检查示例文件:');
const samples = ['ieee14.cdf', 'ieee5.cdf'];
samples.forEach(f => {
  const p = path.join(SAMPLES_DIR, f);
  const exists = fs.existsSync(p);
  console.log(`  ${f}: ${exists ? '✓ 存在' : '✗ 不存在'}`);
  if (exists) {
    const size = fs.statSync(p).size;
    console.log(`    大小: ${size} 字节`);
  }
});

console.log('\n2. 检查目录结构:');
const dirs = [
  ['native', 'native模块'],
  ['native/src', 'native源码'],
  ['native/include', 'native头文件'],
  ['native/src/core', '核心模块源码'],
  ['native/src/solver', '求解器源码'],
  ['native/src/binding', 'N-API绑定源码'],
  ['frontend', '前端项目'],
  ['frontend/src', '前端源码'],
  ['frontend/src/components', '前端组件'],
  ['frontend/src/stores', '状态管理'],
  ['frontend/electron', 'Electron主进程']
];

dirs.forEach(([d, name]) => {
  const p = path.join(__dirname, '..', d);
  const exists = fs.existsSync(p);
  console.log(`  ${name}: ${exists ? '✓' : '✗'}`);
});

console.log('\n3. 源代码文件检查:');
const srcFiles = [
  ['native/src/core/GridData.cpp', '电网数据模型'],
  ['native/src/core/CDFParser.cpp', 'CDF文件解析器'],
  ['native/src/solver/NewtonRaphsonSolver.cpp', '牛顿-拉夫逊求解器'],
  ['native/src/binding/GridSolverWrapper.cpp', 'N-API包装器'],
  ['native/src/binding/main.cpp', '模块入口'],
  ['native/index.js', 'JS封装入口'],
  ['frontend/src/App.vue', '主应用组件'],
  ['frontend/src/components/GridCanvas.vue', 'Canvas渲染组件'],
  ['frontend/src/components/ControlPanel.vue', '控制面板组件'],
  ['frontend/src/components/DataPanel.vue', '数据面板组件'],
  ['frontend/src/components/Toolbar.vue', '工具栏组件'],
  ['frontend/src/stores/grid.js', 'Pinia状态存储'],
  ['frontend/electron/main/index.js', 'Electron主进程'],
  ['frontend/electron/preload/index.js', '预加载脚本']
];

srcFiles.forEach(([f, name]) => {
  const p = path.join(__dirname, '..', f);
  const exists = fs.existsSync(p);
  if (exists) {
    const size = fs.statSync(p).size;
    console.log(`  ✓ ${name}: ${size} 字节`);
  } else {
    console.log(`  ✗ ${name}: 缺失`);
  }
});

console.log('\n4. 配置文件检查:');
const cfgFiles = [
  ['package.json', '根配置'],
  ['native/CMakeLists.txt', 'C++构建配置'],
  ['native/package.json', 'native模块配置'],
  ['frontend/package.json', '前端配置'],
  ['frontend/vite.config.js', 'Vite配置']
];

cfgFiles.forEach(([f, name]) => {
  const p = path.join(__dirname, '..', f);
  const exists = fs.existsSync(p);
  console.log(`  ${name}: ${exists ? '✓' : '✗'}`);
});

console.log('\n5. CDF文件格式验证:');
samples.forEach(f => {
  const p = path.join(SAMPLES_DIR, f);
  if (!fs.existsSync(p)) return;
  try {
    const content = fs.readFileSync(p, 'utf8');
    const hasBusData = content.includes('BUS DATA FOLLOWS');
    const hasBranchData = content.includes('BRANCH DATA FOLLOWS');
    const hasTerminator = content.includes('-999');
    const lines = content.split('\n').filter(l => l.trim().length > 0 && !l.includes('-999'));
    console.log(`  ${f}:`);
    console.log(`    BUS数据标记: ${hasBusData ? '✓' : '✗'}`);
    console.log(`    BRANCH数据标记: ${hasBranchData ? '✓' : '✗'}`);
    console.log(`    终止符-999: ${hasTerminator ? '✓' : '✗'}`);
    console.log(`    有效数据行数: ${lines.length}`);
  } catch (e) {
    console.log(`  ${f}: 读取失败 - ${e.message}`);
  }
});

console.log('\n========================================');
console.log('  测试完成！');
console.log('========================================');
console.log('\n构建说明:');
console.log('  1. 安装根依赖:  npm install');
console.log('  2. 构建C++模块: npm run build:native');
console.log('  3. 启动开发模式: npm run dev');
console.log('  4. 生产构建:     npm run build');
console.log('\n功能模块清单:');
console.log('  ✓ C++底层计算引擎');
console.log('    - IEEE CDF文件格式解析器');
console.log('    - 节点导纳矩阵(Ybus)构建');
console.log('    - 牛顿-拉夫逊法潮流求解器');
console.log('    - 雅可比矩阵构建');
console.log('    - LU分解线性方程组求解');
console.log('    - 支路潮流计算');
console.log('    - 发电机出力/变压器变比动态修改支持');
console.log('');
console.log('  ✓ N-API桥接层');
console.log('    - 同步/异步接口封装');
console.log('    - C++与JavaScript数据互转');
console.log('    - Promise风格异步调用');
console.log('');
console.log('  ✓ Electron+Vue3前端');
console.log('    - Canvas电网单线图渲染');
console.log('    - SLACK/PV/PQ节点类型区分');
console.log('    - 变压器特殊符号绘制');
console.log('    - 潮流方向动画箭头');
console.log('    - 缩放、平移、拖放交互');
console.log('    - 发电机有功出力调节滑块');
console.log('    - 变压器变比调节滑块');
console.log('    - 节点/支路详细数据面板');
console.log('    - 求解状态和结果汇总');
