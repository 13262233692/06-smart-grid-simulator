<template>
  <div class="data-panel">
    <div class="tabs">
      <button
        v-for="tab in tabs"
        :key="tab.id"
        class="tab-btn"
        :class="{ active: activeTab === tab.id }"
        @click="activeTab = tab.id"
      >
        <span>{{ tab.icon }}</span>
        {{ tab.label }}
      </button>
    </div>

    <div class="tab-content">
      <!-- 节点详情 -->
      <div v-if="activeTab === 'bus' && store.selectedBus !== null" class="detail-section">
        <div class="section-header">
          <h3>节点详情</h3>
          <span class="badge" :class="selectedBusTypeBadge">
            {{ selectedBus?.typeName }}
          </span>
        </div>
        <div v-if="selectedBus" class="detail-grid">
          <div class="detail-item">
            <span class="label">节点编号</span>
            <span class="value">#{{ selectedBus.id }}</span>
          </div>
          <div class="detail-item">
            <span class="label">节点名称</span>
            <span class="value">{{ selectedBus.name }}</span>
          </div>
          <div class="detail-item">
            <span class="label">电压幅值</span>
            <span class="value" :class="getVoltageClass(selectedBus)">
              {{ selectedBus.Vm.toFixed(4) }} pu
            </span>
          </div>
          <div class="detail-item">
            <span class="label">电压相角</span>
            <span class="value">{{ selectedBus.Va.toFixed(3) }}°</span>
          </div>
          <div class="detail-item">
            <span class="label">基准电压</span>
            <span class="value">{{ selectedBus.baseKV }} kV</span>
          </div>
          <div class="detail-item">
            <span class="label">区域/分区</span>
            <span class="value">{{ selectedBus.area }} / {{ selectedBus.zone }}</span>
          </div>

          <div class="detail-item full">
            <span class="label">有功功率</span>
          </div>
          <div class="detail-item">
            <span class="label">负荷 Pd</span>
            <span class="value">{{ selectedBus.Pd.toFixed(2) }} MW</span>
          </div>
          <div class="detail-item">
            <span class="label">发电 Pg</span>
            <span class="value ok">{{ selectedBus.Pg.toFixed(2) }} MW</span>
          </div>

          <div class="detail-item full">
            <span class="label">无功功率</span>
          </div>
          <div class="detail-item">
            <span class="label">负荷 Qd</span>
            <span class="value">{{ selectedBus.Qd.toFixed(2) }} MVar</span>
          </div>
          <div class="detail-item">
            <span class="label">发电 Qg</span>
            <span class="value ok">{{ selectedBus.Qg.toFixed(2) }} MVar</span>
          </div>

          <div class="detail-item full">
            <span class="label">电压限制</span>
          </div>
          <div class="detail-item">
            <span class="label">Vmin</span>
            <span class="value">{{ selectedBus.VmMin.toFixed(2) }} pu</span>
          </div>
          <div class="detail-item">
            <span class="label">Vmax</span>
            <span class="value">{{ selectedBus.VmMax.toFixed(2) }} pu</span>
          </div>

          <div v-if="selectedBus.shuntG || selectedBus.shuntB" class="detail-item full">
            <span class="label">并联补偿 G/B</span>
            <span class="value">
              {{ selectedBus.shuntG.toFixed(2) }} / {{ selectedBus.shuntB.toFixed(2) }}
            </span>
          </div>
        </div>
      </div>

      <div v-else-if="activeTab === 'branch' && store.selectedBranch !== null" class="detail-section">
        <div class="section-header">
          <h3>支路详情</h3>
          <span class="badge" :class="selectedBranch?.isTransformer ? 'badge-warning' : 'badge-info'">
            {{ selectedBranch?.isTransformer ? '变压器' : '线路' }}
          </span>
        </div>
        <div v-if="selectedBranch" class="detail-grid">
          <div class="detail-item">
            <span class="label">支路编号</span>
            <span class="value">#{{ selectedBranch.id }}</span>
          </div>
          <div class="detail-item">
            <span class="label">连接节点</span>
            <span class="value">{{ selectedBranch.fromBus }} → {{ selectedBranch.toBus }}</span>
          </div>
          <div class="detail-item">
            <span class="label">电阻 R</span>
            <span class="value">{{ selectedBranch.r.toFixed(5) }} pu</span>
          </div>
          <div class="detail-item">
            <span class="label">电抗 X</span>
            <span class="value">{{ selectedBranch.x.toFixed(5) }} pu</span>
          </div>
          <div class="detail-item">
            <span class="label">电纳 B</span>
            <span class="value">{{ selectedBranch.b.toFixed(5) }} pu</span>
          </div>
          <div class="detail-item">
            <span class="label">运行状态</span>
            <span class="value" :class="selectedBranch.status ? 'ok' : 'err'">
              {{ selectedBranch.status ? '运行' : '停运' }}
            </span>
          </div>
          <div v-if="selectedBranch.isTransformer" class="detail-item">
            <span class="label">变比 Tap</span>
            <span class="value highlight">{{ selectedBranch.tap.toFixed(3) }}</span>
          </div>
          <div v-if="selectedBranch.isTransformer" class="detail-item">
            <span class="label">相移</span>
            <span class="value">{{ selectedBranch.phaseShift.toFixed(2) }}°</span>
          </div>
          <div class="detail-item">
            <span class="label">额定容量 A</span>
            <span class="value">{{ selectedBranch.rateA }} MVA</span>
          </div>
          <div class="detail-item">
            <span class="label">额定容量 B</span>
            <span class="value">{{ selectedBranch.rateB }} MVA</span>
          </div>
        </div>

        <div v-if="selectedFlow" class="flow-section">
          <div class="section-header">
            <h3>潮流结果</h3>
          </div>
          <div class="detail-grid">
            <div class="detail-item">
              <span class="label">P (From)</span>
              <span class="value">{{ selectedFlow.Pfrom.toFixed(2) }} MW</span>
            </div>
            <div class="detail-item">
              <span class="label">Q (From)</span>
              <span class="value">{{ selectedFlow.Qfrom.toFixed(2) }} MVar</span>
            </div>
            <div class="detail-item">
              <span class="label">P (To)</span>
              <span class="value">{{ selectedFlow.Pto.toFixed(2) }} MW</span>
            </div>
            <div class="detail-item">
              <span class="label">Q (To)</span>
              <span class="value">{{ selectedFlow.Qto.toFixed(2) }} MVar</span>
            </div>
            <div class="detail-item full loss">
              <span class="label">有功损耗</span>
              <span class="value err">{{ selectedFlow.Ploss.toFixed(3) }} MW</span>
            </div>
            <div class="detail-item full loss">
              <span class="label">无功损耗</span>
              <span class="value err">{{ selectedFlow.Qloss.toFixed(3) }} MVar</span>
            </div>
          </div>
        </div>
      </div>

      <div v-else class="detail-section">
        <div v-if="!store.isLoaded" class="empty-detail">
          <div class="empty-icon">📋</div>
          <p>在画布上点击节点或支路查看详情</p>
        </div>

        <div v-else class="summary-section">
          <div class="section-header">
            <h3>系统概览</h3>
          </div>
          <div class="summary-grid">
            <div class="summary-card">
              <div class="summary-icon slack">⚖️</div>
              <div class="summary-info">
                <div class="summary-label">平衡节点</div>
                <div class="summary-value">{{ store.statistics?.nSlack || 0 }}</div>
              </div>
            </div>
            <div class="summary-card">
              <div class="summary-icon pv">🔌</div>
              <div class="summary-info">
                <div class="summary-label">PV 节点</div>
                <div class="summary-value">{{ store.statistics?.nPV || 0 }}</div>
              </div>
            </div>
            <div class="summary-card">
              <div class="summary-icon pq">🏠</div>
              <div class="summary-info">
                <div class="summary-label">PQ 节点</div>
                <div class="summary-value">{{ store.statistics?.nPQ || 0 }}</div>
              </div>
            </div>
            <div class="summary-card">
              <div class="summary-icon branch">🔗</div>
              <div class="summary-info">
                <div class="summary-label">支路数</div>
                <div class="summary-value">{{ store.statistics?.nBranch || 0 }}</div>
              </div>
            </div>
          </div>

          <div v-if="store.isSolved" class="result-summary">
            <div class="section-header">
              <h3>求解结果汇总</h3>
            </div>
            <div class="detail-grid">
              <div class="detail-item">
                <span class="label">收敛状态</span>
                <span class="value" :class="store.solution.converged ? 'ok' : 'err'">
                  {{ store.solution.converged ? '已收敛 ✓' : '未收敛 ✗' }}
                </span>
              </div>
              <div class="detail-item">
                <span class="label">迭代次数</span>
                <span class="value">{{ store.solution.iterations }}</span>
              </div>
              <div class="detail-item">
                <span class="label">计算耗时</span>
                <span class="value">{{ store.solution.elapsedMs.toFixed(2) }} ms</span>
              </div>
              <div class="detail-item">
                <span class="label">容差设置</span>
                <span class="value">{{ store.solution.tolerance.toExponential() }}</span>
              </div>
              <div class="detail-item full loss">
                <span class="label">总有功损耗</span>
                <span class="value err">{{ store.solution.totalLossesP?.toFixed(3) }} MW</span>
              </div>
              <div class="detail-item full loss">
                <span class="label">总无功损耗</span>
                <span class="value err">{{ store.solution.totalLossesQ?.toFixed(3) }} MVar</span>
              </div>
            </div>
          </div>
        </div>
      </div>

      <!-- 母线列表 -->
      <div v-if="activeTab === 'buses'" class="list-section">
        <div class="section-header">
          <h3>母线列表 ({{ store.buses.length }})</h3>
        </div>
        <div class="data-table-wrapper">
          <table class="data-table">
            <thead>
              <tr>
                <th>ID</th>
                <th>类型</th>
                <th>Vm (pu)</th>
                <th>Va (°)</th>
                <th>Pg (MW)</th>
                <th>Pd (MW)</th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="bus in store.buses"
                :key="bus.id"
                :class="{ selected: store.selectedBus === bus.id }"
                @click="store.selectBus(bus.id)"
              >
                <td>{{ bus.id }}</td>
                <td>
                  <span class="bus-type-dot" :class="bus.typeName.toLowerCase()"></span>
                  {{ bus.typeName }}
                </td>
                <td :class="getVoltageClass(bus)">{{ bus.Vm.toFixed(3) }}</td>
                <td>{{ bus.Va.toFixed(2) }}</td>
                <td class="num">{{ bus.Pg.toFixed(1) }}</td>
                <td class="num">{{ bus.Pd.toFixed(1) }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- 支路列表 -->
      <div v-if="activeTab === 'branches'" class="list-section">
        <div class="section-header">
          <h3>支路列表 ({{ store.branches.length }})</h3>
        </div>
        <div class="data-table-wrapper">
          <table class="data-table">
            <thead>
              <tr>
                <th>ID</th>
                <th>From→To</th>
                <th>类型</th>
                <th>R</th>
                <th>Tap</th>
                <th>Ploss</th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="branch in store.branches"
                :key="branch.id"
                :class="{ selected: store.selectedBranch === branch.id }"
                @click="store.selectBranch(branch.id)"
              >
                <td>{{ branch.id }}</td>
                <td>{{ branch.fromBus }}→{{ branch.toBus }}</td>
                <td>
                  {{ branch.isTransformer ? '⚡ 变压器' : '─ 线路' }}
                </td>
                <td>{{ branch.r.toFixed(4) }}</td>
                <td>{{ branch.tap.toFixed(3) }}</td>
                <td class="num err" v-if="getBranchFlow(branch.id)">{{ getBranchFlow(branch.id).Ploss.toFixed(3) }}</td>
                <td class="num" v-else>-</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useGridStore } from '../stores/grid'

const store = useGridStore()

const tabs = [
  { id: 'detail', icon: '📋', label: '详情' },
  { id: 'bus', icon: '⚡', label: '节点' },
  { id: 'branch', icon: '🔗', label: '支路' },
  { id: 'buses', icon: '📊', label: '母线表' },
  { id: 'branches', icon: '📈', label: '支路表' }
]

const activeTab = ref('detail')

const selectedBus = computed(() => {
  if (store.selectedBus === null) return null
  return store.buses.find(b => b.id === store.selectedBus)
})

const selectedBranch = computed(() => {
  if (store.selectedBranch === null) return null
  return store.branches.find(b => b.id === store.selectedBranch)
})

const selectedFlow = computed(() => {
  if (!selectedBranch.value || !store.solution?.branchFlows) return null
  const b = selectedBranch.value
  return store.solution.branchFlows.find(
    f => (f.fromBus === b.fromBus && f.toBus === b.toBus) ||
         (f.fromBus === b.toBus && f.toBus === b.fromBus)
  )
})

const selectedBusTypeBadge = computed(() => {
  if (!selectedBus.value) return ''
  const type = selectedBus.value.type
  if (type === 0) return 'badge-success'
  if (type === 1) return 'badge-warning'
  return 'badge-info'
})

function getVoltageClass(bus) {
  if (!bus) return ''
  if (bus.Vm < bus.VmMin || bus.Vm > bus.VmMax) return 'err'
  const mid = (bus.VmMin + bus.VmMax) / 2
  if (Math.abs(bus.Vm - mid) / (bus.VmMax - bus.VmMin) > 0.35) return 'warn'
  return 'ok'
}

function getBranchFlow(branchId) {
  if (!store.solution?.branchFlows) return null
  const branch = store.branches.find(b => b.id === branchId)
  if (!branch) return null
  return store.solution.branchFlows.find(
    f => (f.fromBus === branch.fromBus && f.toBus === branch.toBus) ||
         (f.fromBus === branch.toBus && f.toBus === branch.fromBus)
  )
}
</script>

<style scoped>
.data-panel {
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.tabs {
  display: flex;
  padding: 8px;
  gap: 4px;
  background-color: var(--bg-primary);
  border-bottom: 1px solid var(--border-color);
}

.tab-btn {
  flex: 1;
  padding: 8px 6px;
  background-color: transparent;
  color: var(--text-muted);
  border-radius: 6px;
  font-size: 12px;
  font-weight: 500;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  transition: all 0.2s;
  gap: 4px;
}

.tab-btn:hover {
  background-color: var(--bg-tertiary);
  color: var(--text-secondary);
}

.tab-btn.active {
  background-color: var(--accent);
  color: white;
}

.tab-btn span:first-child {
  font-size: 16px;
}

.tab-content {
  flex: 1;
  overflow-y: auto;
  padding: 12px;
}

.section-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 0;
  margin-bottom: 12px;
  border-bottom: 1px solid var(--border-color);
  padding-bottom: 8px;
}

.section-header h3 {
  font-size: 14px;
  font-weight: 600;
  color: var(--text-primary);
}

.empty-detail {
  text-align: center;
  padding: 60px 20px;
  color: var(--text-muted);
}

.empty-icon {
  font-size: 48px;
  margin-bottom: 16px;
}

.summary-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  margin-bottom: 20px;
}

.summary-card {
  background-color: var(--bg-tertiary);
  border-radius: 8px;
  padding: 12px;
  display: flex;
  align-items: center;
  gap: 10px;
}

.summary-icon {
  width: 40px;
  height: 40px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 20px;
}

.summary-icon.slack { background-color: rgba(34, 197, 94, 0.15); }
.summary-icon.pv { background-color: rgba(245, 158, 11, 0.15); }
.summary-icon.pq { background-color: rgba(59, 130, 246, 0.15); }
.summary-icon.branch { background-color: rgba(6, 182, 212, 0.15); }

.summary-label {
  font-size: 11px;
  color: var(--text-secondary);
}

.summary-value {
  font-size: 18px;
  font-weight: 700;
  color: var(--text-primary);
}

.detail-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px 12px;
}

.detail-item {
  display: flex;
  justify-content: space-between;
  padding: 6px 0;
  border-bottom: 1px solid rgba(71, 85, 105, 0.3);
  align-items: center;
}

.detail-item.full {
  grid-column: 1 / -1;
  background-color: rgba(59, 130, 246, 0.05);
  padding: 8px 10px;
  border-radius: 6px;
  margin-top: 4px;
  border-bottom: none;
}

.detail-item.loss {
  background-color: rgba(239, 68, 68, 0.05);
}

.detail-item .label {
  font-size: 12px;
  color: var(--text-secondary);
}

.detail-item .value {
  font-size: 13px;
  font-weight: 500;
  font-family: 'Consolas', monospace;
  color: var(--text-primary);
}

.detail-item .value.ok { color: var(--success); }
.detail-item .value.warn { color: var(--warning); }
.detail-item .value.err { color: var(--danger); }
.detail-item .value.highlight {
  color: var(--accent);
  font-weight: 700;
}

.flow-section {
  margin-top: 20px;
}

.list-section {
  padding-top: 4px;
}

.data-table-wrapper {
  max-height: 500px;
  overflow-y: auto;
  overflow-x: auto;
}

.data-table {
  width: 100%;
  font-size: 12px;
}

.data-table th,
.data-table td {
  padding: 8px 8px;
  text-align: left;
  border-bottom: 1px solid var(--border-color);
  white-space: nowrap;
}

.data-table th {
  position: sticky;
  top: 0;
  background-color: var(--bg-tertiary);
  font-weight: 600;
  color: var(--text-secondary);
  z-index: 1;
}

.data-table tbody tr {
  cursor: pointer;
  transition: background-color 0.15s;
}

.data-table tbody tr:hover {
  background-color: rgba(59, 130, 246, 0.08);
}

.data-table tbody tr.selected {
  background-color: rgba(239, 68, 68, 0.12);
}

.data-table td.num {
  font-family: 'Consolas', monospace;
  text-align: right;
}

.data-table td.num.err { color: var(--danger); }

.bus-type-dot {
  display: inline-block;
  width: 8px;
  height: 8px;
  border-radius: 50%;
  margin-right: 6px;
  vertical-align: middle;
}

.bus-type-dot.slack { background-color: #22c55e; }
.bus-type-dot.pv { background-color: #f59e0b; }
.bus-type-dot.pq { background-color: #3b82f6; }
</style>
