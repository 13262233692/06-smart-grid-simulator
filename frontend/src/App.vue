<template>
  <div class="app-container">
    <Toolbar />
    <div class="main-content">
      <aside class="left-panel">
        <ControlPanel />
      </aside>
      <main class="canvas-area">
        <GridCanvas />
        <div v-if="store.loading || store.computing" class="loading-overlay">
          <div class="loading-spinner"></div>
          <p>{{ loadingText }}</p>
          <div v-if="store.computationProgress" class="computation-detail">
            迭代 {{ store.computationProgress.currentIteration }}/{{ store.computationProgress.maxIterations }}
            · 失配 {{ store.computationProgress.maxMismatch?.toExponential(3) }}
          </div>
          <button v-if="store.computing" class="btn-cancel-overlay" @click="store.cancelComputation()">取消计算</button>
        </div>
      </main>
      <aside class="right-panel">
        <DataPanel />
      </aside>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useGridStore } from './stores/grid'
import Toolbar from './components/Toolbar.vue'
import ControlPanel from './components/ControlPanel.vue'
import GridCanvas from './components/GridCanvas.vue'
import DataPanel from './components/DataPanel.vue'

const store = useGridStore()

const loadingText = computed(() => {
  if (store.solution) return '正在修改并重新计算潮流...'
  return store.isLoaded ? '正在进行牛顿-拉夫逊迭代计算...' : '正在解析 IEEE CDF 文件...'
})
</script>

<style scoped>
.app-container {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.main-content {
  flex: 1;
  display: flex;
  overflow: hidden;
}

.left-panel {
  width: 320px;
  min-width: 280px;
  max-width: 400px;
  border-right: 1px solid var(--border-color);
  overflow-y: auto;
  background-color: var(--bg-secondary);
}

.canvas-area {
  flex: 1;
  position: relative;
  overflow: hidden;
  background:
    radial-gradient(circle at 50% 50%, rgba(59, 130, 246, 0.03) 0%, transparent 70%),
    var(--bg-primary);
}

.right-panel {
  width: 380px;
  min-width: 320px;
  max-width: 480px;
  border-left: 1px solid var(--border-color);
  overflow-y: auto;
  background-color: var(--bg-secondary);
}

.loading-overlay {
  position: absolute;
  inset: 0;
  background-color: rgba(15, 23, 42, 0.7);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 16px;
  backdrop-filter: blur(4px);
  z-index: 1000;
}

.loading-overlay p {
  color: var(--text-primary);
  font-size: 16px;
}

.computation-detail {
  color: var(--text-secondary);
  font-size: 13px;
  font-family: 'Consolas', monospace;
}

.btn-cancel-overlay {
  padding: 8px 24px;
  background-color: var(--danger);
  color: white;
  border: none;
  border-radius: 6px;
  font-size: 14px;
  cursor: pointer;
}

.btn-cancel-overlay:hover {
  opacity: 0.85;
}

.loading-spinner {
  width: 48px;
  height: 48px;
  border: 4px solid var(--bg-tertiary);
  border-top-color: var(--accent);
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
</style>
