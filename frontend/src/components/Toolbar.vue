<template>
  <div class="toolbar">
    <div class="toolbar-title">
      <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z" />
      </svg>
      电网潮流计算系统
    </div>

    <button class="btn-secondary" @click="onOpenFile">
      <span style="margin-right:6px;">📂</span>
      打开 CDF 文件
    </button>

    <button class="btn-secondary" @click="onLoadSample">
      <span style="margin-right:6px;">📊</span>
      加载示例算例
    </button>

    <div class="divider"></div>

    <button class="btn-success" :disabled="!store.isLoaded || store.loading || store.computing" @click="onSolve">
      <span style="margin-right:6px;">⚡</span>
      潮流计算
    </button>

    <button class="btn-secondary" :disabled="!store.isSolved" @click="onReset">
      🔄 重置
    </button>

    <div class="toolbar-spacer"></div>

    <div v-if="store.isLoaded" class="grid-stats">
      <span class="stat-item">
        <span class="stat-label">算例:</span>
        <span class="stat-value">{{ store.statistics?.caseName }}</span>
      </span>
      <span class="stat-item">
        <span class="stat-label">节点:</span>
        <span class="stat-value">{{ store.statistics?.nBus }}</span>
      </span>
      <span class="stat-item">
        <span class="stat-label">支路:</span>
        <span class="stat-value">{{ store.statistics?.nBranch }}</span>
      </span>
      <span class="stat-item">
        <span class="stat-label">发电机:</span>
        <span class="stat-value">{{ store.statistics?.nGen }}</span>
      </span>
    </div>

    <div class="divider"></div>

    <span class="badge" :class="statusBadgeClass">
      {{ statusText }}
    </span>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useGridStore } from '../stores/grid'

const store = useGridStore()

const statusText = computed(() => {
  if (store.computing) return '计算中...'
  if (store.loading) return '加载中...'
  if (store.error) return '错误'
  if (store.isSolved && store.solution?.converged) return '已收敛'
  if (store.isSolved && !store.solution?.converged) return '未收敛'
  if (store.isLoaded) return '已加载'
  return '待加载'
})

const statusBadgeClass = computed(() => {
  if (store.computing || store.loading) return 'badge-info'
  if (store.error) return 'badge-danger'
  if (store.isSolved && store.solution?.converged) return 'badge-success'
  if (store.isSolved && !store.solution?.converged) return 'badge-warning'
  if (store.isLoaded) return 'badge-info'
  return 'badge-warning'
})

async function onOpenFile() {
  const result = await window.electronAPI.dialog.openFile()
  if (result.success) {
    await store.loadCDFFile(result.filePath)
  }
}

async function onLoadSample() {
  const samples = await window.electronAPI.grid.getSampleDataPath()
  if (samples.success && samples.files && samples.files.length > 0) {
    await store.loadCDFFile(samples.files[0].path)
  } else {
    alert('未找到示例算例文件。请在 samples/ 目录下放置 IEEE 标准算例文件。')
  }
}

async function onSolve() {
  await store.solvePowerFlow()
}

function onReset() {
  store.reset()
}
</script>

<style scoped>
.grid-stats {
  display: flex;
  align-items: center;
  gap: 16px;
  font-size: 13px;
}

.stat-item {
  display: flex;
  align-items: center;
  gap: 4px;
}

.stat-label {
  color: var(--text-secondary);
}

.stat-value {
  color: var(--text-primary);
  font-weight: 600;
}
</style>
