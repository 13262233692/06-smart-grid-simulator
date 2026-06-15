<template>
  <div class="control-panel">
    <div class="panel-header">
      <span>⚙️ 控制面板</span>
    </div>

    <div v-if="store.computing" class="computation-status">
      <div class="progress-bar-container">
        <div class="progress-bar" :style="{ width: store.progressPercent + '%' }"></div>
      </div>
      <div class="progress-info">
        <span v-if="store.computationProgress">
          迭代 {{ store.computationProgress.currentIteration }}/{{ store.computationProgress.maxIterations }}
          · 失配 {{ store.computationProgress.maxMismatch?.toExponential(2) }}
        </span>
        <span v-else>计算中...</span>
        <button class="btn-cancel" @click="store.cancelComputation()">取消</button>
      </div>
    </div>

    <div v-if="!store.isLoaded" class="panel-body empty-state">
      <div class="empty-icon">📊</div>
      <h3>尚未加载电网数据</h3>
      <p>请通过顶部工具栏打开 IEEE CDF 格式文件或加载示例算例</p>
    </div>

    <template v-else>
      <div class="panel-section">
        <div class="section-title">
          <span>🔧 求解器配置</span>
        </div>
        <div class="panel-body">
          <div class="input-group">
            <label>收敛容差 (Tolerance)</label>
            <input
              type="number"
              step="1e-8"
              :value="store.solverConfig.tolerance"
              @change="(e) => store.solverConfig.tolerance = parseFloat(e.target.value)"
            />
          </div>
          <div class="input-group">
            <label>最大迭代次数</label>
            <input
              type="number"
              min="1"
              max="200"
              :value="store.solverConfig.maxIterations"
              @change="(e) => store.solverConfig.maxIterations = parseInt(e.target.value)"
            />
          </div>
          <div class="input-group">
            <label>超时时间 (ms)</label>
            <input
              type="number"
              min="1000"
              step="1000"
              :value="store.solverConfig.timeoutMs"
              @change="(e) => store.solverConfig.timeoutMs = parseInt(e.target.value)"
            />
          </div>
        </div>
      </div>

      <div class="panel-section">
        <div class="section-title">
          <span>🔌 发电机调节</span>
          <span class="count-badge">{{ store.generators.length }} 台</span>
        </div>
        <div v-if="store.generators.length === 0" class="panel-body text-muted">
          该电网中无发电机节点
        </div>
        <div v-else class="gen-list">
          <div
            v-for="(gen, idx) in store.generators"
            :key="gen.id"
            class="gen-item"
            :class="{ selected: store.selectedBus === gen.busId }"
            @click="store.selectBus(gen.busId)"
          >
            <div class="gen-header">
              <div class="gen-info">
                <span class="gen-id">G{{ gen.id }}</span>
                <span class="bus-tag">节点 #{{ gen.busId }}</span>
              </div>
              <span class="badge badge-success">PV</span>
            </div>
            <div class="slider-group">
              <label>P (MW)</label>
              <input
                type="range"
                :min="gen.PgMin"
                :max="gen.PgMax"
                :step="0.5"
                :value="genCurrentPg(gen, idx)"
                @input="(e) => onGenPowerInput(idx, gen, parseFloat(e.target.value))"
                @change="(e) => onGenPowerChange(idx, gen, parseFloat(e.target.value))"
              />
              <span class="value">{{ genCurrentPg(gen, idx).toFixed(1) }}</span>
            </div>
            <div class="gen-limits">
              <span>Min: {{ gen.PgMin }}</span>
              <span>Max: {{ gen.PgMax }}</span>
            </div>
          </div>
        </div>
      </div>

      <div class="panel-section">
        <div class="section-title">
          <span>⚡ 变压器调节</span>
          <span class="count-badge">{{ transformers.length }} 台</span>
        </div>
        <div v-if="transformers.length === 0" class="panel-body text-muted">
          该电网中无变压器支路
        </div>
        <div v-else class="transformer-list">
          <div
            v-for="(tr, idx) in transformers"
            :key="tr.id"
            class="transformer-item"
            :class="{ selected: store.selectedBranch === tr.id }"
            @click="store.selectBranch(tr.id)"
          >
            <div class="transformer-header">
              <div class="transformer-info">
                <span class="tr-id">T{{ tr.id }}</span>
                <span class="bus-tag">{{ tr.fromBus }} → {{ tr.toBus }}</span>
              </div>
              <span class="badge badge-warning">变压器</span>
            </div>
            <div class="slider-group">
              <label>变比 Tap</label>
              <input
                type="range"
                :min="tr.ratioMin"
                :max="tr.ratioMax"
                step="0.005"
                :value="trCurrentTap(tr, idx)"
                @input="(e) => onTapInput(idx, tr, parseFloat(e.target.value))"
                @change="(e) => onTapChange(idx, tr, parseFloat(e.target.value))"
              />
              <span class="value">{{ trCurrentTap(tr, idx).toFixed(3) }}</span>
            </div>
            <div class="transformer-limits">
              <span>Min: {{ tr.ratioMin }}</span>
              <span>Max: {{ tr.ratioMax }}</span>
            </div>
          </div>
        </div>
      </div>

      <div class="panel-section" v-if="store.hasPendingChanges">
        <div class="pending-changes">
          <div class="pending-title">
            ⚠️ 待应用修改: {{ store.pendingChangeCount }} 项
          </div>
          <div class="pending-actions">
            <button
              class="btn-primary"
              @click="applyAllChanges"
              :disabled="store.computing"
            >
              应用并重新计算
            </button>
            <button class="btn-secondary" @click="store.clearPendingChanges()">
              取消
            </button>
          </div>
        </div>
      </div>
    </template>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useGridStore } from '../stores/grid'

const store = useGridStore()

const transformers = computed(() =>
  store.branches.filter(b => b.tap !== 1 || b.phaseShift !== 0)
)

function genCurrentPg(gen, idx) {
  const pending = store.pendingGenChanges[idx]
  if (pending !== undefined) return pending
  const buses = store.solution?.buses || store.gridInfo?.buses || []
  const bus = buses.find(b => b.id === gen.busId)
  return bus?.Pg ?? gen.Pg
}

function trCurrentTap(tr, idx) {
  const pending = store.pendingTapChanges[idx]
  if (pending !== undefined) return pending
  return tr.tap
}

function onGenPowerInput(idx, gen, value) {
  if (Math.abs(value - gen.Pg) < 0.01) {
    store.removePendingGenChange(idx)
  } else {
    store.setPendingGenChange(idx, value)
  }
}

function onGenPowerChange(idx, gen, value) {
  if (Math.abs(value - gen.Pg) < 0.01) {
    store.removePendingGenChange(idx)
    return
  }
  store.setPendingGenChange(idx, value)
  store.debouncedSolveWithModifications(
    [[idx, value]],
    []
  )
}

function onTapInput(idx, tr, value) {
  if (Math.abs(value - tr.tap) < 0.001) {
    store.removePendingTapChange(idx)
  } else {
    store.setPendingTapChange(idx, value)
  }
}

function onTapChange(idx, tr, value) {
  if (Math.abs(value - tr.tap) < 0.001) {
    store.removePendingTapChange(idx)
    return
  }
  store.setPendingTapChange(idx, value)
  store.debouncedSolveWithModifications(
    [],
    [[idx, value]]
  )
}

async function applyAllChanges() {
  const genChanges = Object.entries(store.pendingGenChanges).map(([k, v]) => [parseInt(k), v])
  const tapChanges = Object.entries(store.pendingTapChanges).map(([k, v]) => [parseInt(k), v])
  store.clearPendingChanges()
  await store.solveWithModifications(genChanges, tapChanges)
}
</script>

<style scoped>
.control-panel {
  height: 100%;
  overflow-y: auto;
}

.computation-status {
  padding: 12px 16px;
  background-color: rgba(59, 130, 246, 0.1);
  border-bottom: 1px solid var(--border-color);
}

.progress-bar-container {
  width: 100%;
  height: 4px;
  background-color: var(--bg-tertiary);
  border-radius: 2px;
  overflow: hidden;
  margin-bottom: 8px;
}

.progress-bar {
  height: 100%;
  background-color: var(--accent);
  border-radius: 2px;
  transition: width 0.3s ease;
}

.progress-info {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 11px;
  color: var(--text-secondary);
}

.btn-cancel {
  padding: 2px 10px;
  font-size: 11px;
  background-color: var(--danger);
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.btn-cancel:hover {
  opacity: 0.85;
}

.panel-section {
  border-bottom: 1px solid var(--border-color);
}

.section-title {
  padding: 10px 16px;
  font-size: 13px;
  font-weight: 600;
  color: var(--text-primary);
  background-color: rgba(59, 130, 246, 0.08);
  display: flex;
  align-items: center;
  justify-content: space-between;
  border-bottom: 1px solid var(--border-color);
}

.count-badge {
  background-color: var(--bg-tertiary);
  color: var(--text-secondary);
  padding: 2px 8px;
  border-radius: 10px;
  font-size: 11px;
  font-weight: 500;
}

.empty-state {
  text-align: center;
  padding: 40px 20px;
}

.empty-icon {
  font-size: 48px;
  margin-bottom: 16px;
}

.empty-state h3 {
  font-size: 16px;
  margin-bottom: 8px;
  color: var(--text-primary);
}

.empty-state p {
  font-size: 13px;
  color: var(--text-muted);
  line-height: 1.6;
}

.text-muted {
  padding: 20px 16px;
  color: var(--text-muted);
  font-size: 13px;
  text-align: center;
}

.gen-list,
.transformer-list {
  padding: 8px;
}

.gen-item,
.transformer-item {
  padding: 12px;
  margin-bottom: 8px;
  background-color: var(--bg-tertiary);
  border: 1px solid var(--border-color);
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.2s;
}

.gen-item:hover,
.transformer-item:hover {
  border-color: var(--accent);
}

.gen-item.selected,
.transformer-item.selected {
  border-color: var(--danger);
  background-color: rgba(239, 68, 68, 0.08);
}

.gen-header,
.transformer-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 10px;
}

.gen-info,
.transformer-info {
  display: flex;
  align-items: center;
  gap: 8px;
}

.gen-id,
.tr-id {
  font-weight: 700;
  color: var(--text-primary);
  font-size: 14px;
}

.bus-tag {
  background-color: var(--bg-secondary);
  padding: 2px 8px;
  border-radius: 4px;
  font-size: 11px;
  color: var(--text-secondary);
  font-family: 'Consolas', monospace;
}

.slider-group {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 6px;
}

.slider-group label {
  font-size: 11px;
  color: var(--text-secondary);
  min-width: 50px;
  font-weight: 500;
}

.slider-group input[type="range"] {
  flex: 1;
  height: 4px;
  accent-color: var(--accent);
}

.slider-group .value {
  min-width: 55px;
  text-align: right;
  font-family: 'Consolas', monospace;
  color: var(--accent);
  font-size: 12px;
  font-weight: 600;
}

.gen-limits,
.transformer-limits {
  display: flex;
  justify-content: space-between;
  font-size: 10px;
  color: var(--text-muted);
  font-family: 'Consolas', monospace;
}

.pending-changes {
  padding: 16px;
  background-color: rgba(234, 179, 8, 0.1);
  border-top: 1px solid rgba(234, 179, 8, 0.3);
}

.pending-title {
  font-size: 13px;
  font-weight: 600;
  color: var(--warning);
  margin-bottom: 12px;
}

.pending-actions {
  display: flex;
  gap: 8px;
}

.pending-actions button {
  flex: 1;
  padding: 10px 12px;
  font-size: 13px;
}

.pending-actions button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}
</style>
