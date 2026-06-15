<template>
  <div class="grid-canvas-container" ref="containerRef">
    <canvas
      ref="canvasRef"
      @mousedown="onMouseDown"
      @mousemove="onMouseMove"
      @mouseup="onMouseUp"
      @mouseleave="onMouseUp"
      @wheel="onWheel"
      @dblclick="onDblClick"
      @contextmenu.prevent
    ></canvas>

    <div class="canvas-controls">
      <button class="ctrl-btn" @click="zoomIn" title="放大">＋</button>
      <button class="ctrl-btn" @click="zoomOut" title="缩小">－</button>
      <button class="ctrl-btn" @click="resetView" title="重置视图">⌂</button>
      <button
        class="ctrl-btn"
        :class="{ active: store.showFlows }"
        @click="toggleFlows"
        title="显示潮流"
      >
        ⚡
      </button>
      <button
        class="ctrl-btn"
        :class="{ active: store.showVoltages }"
        @click="toggleVoltages"
        title="显示电压"
      >
        V
      </button>
    </div>

    <div class="canvas-legend">
      <div class="legend-item">
        <span class="legend-dot slack"></span>
        <span>平衡节点 (SLACK)</span>
      </div>
      <div class="legend-item">
        <span class="legend-dot pv"></span>
        <span>PV 节点 (发电机)</span>
      </div>
      <div class="legend-item">
        <span class="legend-dot pq"></span>
        <span>PQ 节点 (负荷)</span>
      </div>
      <div class="legend-item">
        <span class="legend-line"></span>
        <span>输电线路</span>
      </div>
      <div class="legend-item">
        <span class="legend-transformer"></span>
        <span>变压器</span>
      </div>
    </div>

    <div v-if="store.isSolved" class="solution-info">
      <div class="info-row">
        <span>收敛状态:</span>
        <span :class="store.solution.converged ? 'ok' : 'err'">
          {{ store.solution.converged ? '已收敛' : '未收敛' }}
        </span>
      </div>
      <div class="info-row">
        <span>迭代次数:</span>
        <span>{{ store.solution.iterations }}</span>
      </div>
      <div class="info-row">
        <span>计算耗时:</span>
        <span>{{ store.solution.elapsedMs.toFixed(2) }} ms</span>
      </div>
      <div class="info-row">
        <span>有功损耗:</span>
        <span class="ok">{{ store.solution.totalLossesP?.toFixed(2) }} MW</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch, nextTick } from 'vue'
import { useGridStore } from '../stores/grid'

const store = useGridStore()

const containerRef = ref(null)
const canvasRef = ref(null)
let ctx = null
let animationId = null
let flowPhase = 0

const viewScale = ref(1)
const viewOffset = ref({ x: 0, y: 0 })
const isPanning = ref(false)
const panStart = ref({ x: 0, y: 0 })
const offsetStart = ref({ x: 0, y: 0 })
const draggingBus = ref(null)
const hoveredBus = ref(null)
const hoveredBranch = ref(null)

const NODE_RADIUS = 18
const BUS_TYPES = {
  0: { name: 'SLACK', color: '#22c55e' },
  1: { name: 'PV', color: '#f59e0b' },
  2: { name: 'PQ', color: '#3b82f6' }
}

function initCanvas() {
  const canvas = canvasRef.value
  if (!canvas) return

  ctx = canvas.getContext('2d')
  resizeCanvas()

  if (!animationId) {
    animate()
  }
}

function resizeCanvas() {
  const canvas = canvasRef.value
  const container = containerRef.value
  if (!canvas || !container) return

  const dpr = window.devicePixelRatio || 1
  const rect = container.getBoundingClientRect()

  canvas.width = rect.width * dpr
  canvas.height = rect.height * dpr
  canvas.style.width = rect.width + 'px'
  canvas.style.height = rect.height + 'px'

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
  render()
}

function screenToWorld(sx, sy) {
  return {
    x: (sx - viewOffset.value.x) / viewScale.value,
    y: (sy - viewOffset.value.y) / viewScale.value
  }
}

function worldToScreen(wx, wy) {
  return {
    x: wx * viewScale.value + viewOffset.value.x,
    y: wy * viewScale.value + viewOffset.value.y
  }
}

function getBusPos(busId) {
  const pos = store.busPositions[busId]
  if (!pos) return { x: 0, y: 0 }
  return pos
}

function drawGrid() {
  const canvas = canvasRef.value
  const rect = canvas.getBoundingClientRect()
  const gridSize = 50 * viewScale.value

  ctx.strokeStyle = 'rgba(71, 85, 105, 0.15)'
  ctx.lineWidth = 1

  const startX = viewOffset.value.x % gridSize
  const startY = viewOffset.value.y % gridSize

  for (let x = startX; x < rect.width; x += gridSize) {
    ctx.beginPath()
    ctx.moveTo(x, 0)
    ctx.lineTo(x, rect.height)
    ctx.stroke()
  }

  for (let y = startY; y < rect.height; y += gridSize) {
    ctx.beginPath()
    ctx.moveTo(0, y)
    ctx.lineTo(rect.width, y)
    ctx.stroke()
  }
}

function drawBranch(branch, flow = null) {
  const from = getBusPos(branch.fromBus)
  const fromPos = worldToScreen(from.x, from.y)
  const to = getBusPos(branch.toBus)
  const toPos = worldToScreen(to.x, to.y)

  const dx = toPos.x - fromPos.x
  const dy = toPos.y - fromPos.y
  const len = Math.sqrt(dx * dx + dy * dy)
  if (len < 1) return

  const nx = dx / len
  const ny = dy / len

  const startX = fromPos.x + nx * NODE_RADIUS * viewScale.value
  const startY = fromPos.y + ny * NODE_RADIUS * viewScale.value
  const endX = toPos.x - nx * NODE_RADIUS * viewScale.value
  const endY = toPos.y - ny * NODE_RADIUS * viewScale.value

  const isHovered = hoveredBranch.value === branch.id
  const isSelected = store.selectedBranch === branch.id
  const isTransformer = branch.tap !== 1 || branch.phaseShift !== 0

  let lineColor = 'rgba(148, 163, 184, 0.7)'
  let lineWidth = 2

  if (isHovered || isSelected) {
    lineColor = isSelected ? '#ef4444' : '#f59e0b'
    lineWidth = 3
  }

  if (flow && store.showFlows) {
    const flowMag = Math.max(Math.abs(flow.Pfrom), Math.abs(flow.Pto))
    const alpha = Math.min(1, 0.4 + flowMag / 100)
    if (flowMag > 0.1) {
      lineColor = `rgba(6, 182, 212, ${alpha})`
      lineWidth = Math.min(6, 2 + flowMag / 30)
    }
  }

  ctx.strokeStyle = lineColor
  ctx.lineWidth = lineWidth * viewScale.value
  ctx.lineCap = 'round'
  ctx.beginPath()
  ctx.moveTo(startX, startY)
  ctx.lineTo(endX, endY)
  ctx.stroke()

  if (isTransformer) {
    const midX = (startX + endX) / 2
    const midY = (startY + endY) / 2
    const perpX = -ny
    const perpY = nx
    const size = 12 * viewScale.value

    ctx.strokeStyle = '#a855f7'
    ctx.lineWidth = 2 * viewScale.value
    ctx.fillStyle = 'rgba(168, 85, 247, 0.2)'

    ctx.beginPath()
    ctx.arc(midX - perpX * size * 0.5, midY - perpY * size * 0.5, size, 0, Math.PI * 2)
    ctx.fill()
    ctx.stroke()

    ctx.beginPath()
    ctx.arc(midX + perpX * size * 0.5, midY + perpY * size * 0.5, size, 0, Math.PI * 2)
    ctx.fill()
    ctx.stroke()
  }

  if (flow && store.showFlows) {
    const direction = flow.Pfrom > 0 ? 1 : -1
    const midT = 0.5 + Math.sin(flowPhase * direction) * 0.3
    const arrowX = startX + (endX - startX) * midT
    const arrowY = startY + (endY - startY) * midT
    const arrowSize = 8 * viewScale.value

    ctx.fillStyle = 'rgba(6, 182, 212, 0.9)'
    ctx.beginPath()
    if (direction > 0) {
      ctx.moveTo(arrowX, arrowY)
      ctx.lineTo(arrowX - nx * arrowSize - ny * arrowSize * 0.5, arrowY - ny * arrowSize + nx * arrowSize * 0.5)
      ctx.lineTo(arrowX - nx * arrowSize + ny * arrowSize * 0.5, arrowY - ny * arrowSize - nx * arrowSize * 0.5)
    } else {
      ctx.moveTo(arrowX, arrowY)
      ctx.lineTo(arrowX + nx * arrowSize - ny * arrowSize * 0.5, arrowY + ny * arrowSize + nx * arrowSize * 0.5)
      ctx.lineTo(arrowX + nx * arrowSize + ny * arrowSize * 0.5, arrowY + ny * arrowSize - nx * arrowSize * 0.5)
    }
    ctx.closePath()
    ctx.fill()
  }
}

function drawBus(bus) {
  const p = getBusPos(bus.id)
  const pos = worldToScreen(p.x, p.y)
  const typeInfo = BUS_TYPES[bus.type] || BUS_TYPES[2]
  const isHovered = hoveredBus.value === bus.id
  const isSelected = store.selectedBus === bus.id
  const radius = NODE_RADIUS * viewScale.value

  if (isSelected) {
    ctx.beginPath()
    ctx.arc(pos.x, pos.y, radius + 6 * viewScale.value, 0, Math.PI * 2)
    ctx.strokeStyle = '#ef4444'
    ctx.lineWidth = 2 * viewScale.value
    ctx.stroke()
  }

  const gradient = ctx.createRadialGradient(pos.x, pos.y, 0, pos.x, pos.y, radius)
  gradient.addColorStop(0, typeInfo.color)
  gradient.addColorStop(1, adjustColor(typeInfo.color, -40))

  ctx.beginPath()
  ctx.arc(pos.x, pos.y, radius, 0, Math.PI * 2)
  ctx.fillStyle = gradient
  ctx.fill()

  ctx.strokeStyle = isHovered ? '#ffffff' : adjustColor(typeInfo.color, -60)
  ctx.lineWidth = 2 * viewScale.value
  ctx.stroke()

  ctx.fillStyle = '#ffffff'
  ctx.font = `bold ${12 * viewScale.value}px Consolas, monospace`
  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.fillText(String(bus.id), pos.x, pos.y)

  if (store.showVoltages && bus.Vm > 0) {
    const label = `${bus.Vm.toFixed(2)} pu`
    ctx.font = `${11 * viewScale.value}px Consolas, monospace`
    ctx.fillStyle = getVoltageColor(bus.Vm, bus.VmMin, bus.VmMax)
    ctx.textAlign = 'center'
    ctx.fillText(label, pos.x, pos.y + radius + 16 * viewScale.value)
  }

  ctx.font = `${10 * viewScale.value}px sans-serif`
  ctx.fillStyle = 'rgba(148, 163, 184, 0.9)'
  ctx.fillText(bus.name || `Bus ${bus.id}`, pos.x, pos.y - radius - 12 * viewScale.value)
}

function adjustColor(hex, amount) {
  const num = parseInt(hex.slice(1), 16)
  let r = Math.max(0, Math.min(255, (num >> 16) + amount))
  let g = Math.max(0, Math.min(255, ((num >> 8) & 0x00FF) + amount))
  let b = Math.max(0, Math.min(255, (num & 0x0000FF) + amount))
  return `rgb(${r}, ${g}, ${b})`
}

function getVoltageColor(vm, vmin, vmax) {
  const norm = (vm - vmin) / (vmax - vmin)
  if (norm < 0 || norm > 1) return '#ef4444'
  if (norm < 0.25) return '#f59e0b'
  if (norm > 0.75) return '#f59e0b'
  return '#22c55e'
}

function render() {
  if (!ctx) return

  const canvas = canvasRef.value
  const rect = canvas.getBoundingClientRect()

  ctx.clearRect(0, 0, rect.width, rect.height)

  drawGrid()

  const flows = {}
  if (store.isSolved && store.solution.branchFlows) {
    for (const f of store.solution.branchFlows) {
      flows[`${f.fromBus}-${f.toBus}`] = f
    }
  }

  for (const branch of store.branches) {
    const flow = flows[`${branch.fromBus}-${branch.toBus}`] || flows[`${branch.toBus}-${branch.fromBus}`]
    drawBranch(branch, flow)
  }

  for (const bus of store.buses) {
    drawBus(bus)
  }
}

function animate() {
  flowPhase += 0.05
  render()
  animationId = requestAnimationFrame(animate)
}

function onMouseDown(e) {
  const rect = canvasRef.value.getBoundingClientRect()
  const sx = e.clientX - rect.left
  const sy = e.clientY - rect.top
  const world = screenToWorld(sx, sy)

  const clickedBus = findBusAt(world.x, world.y)
  if (clickedBus) {
    if (e.button === 0) {
      draggingBus.value = clickedBus.id
      store.selectBus(clickedBus.id)
    }
    return
  }

  const clickedBranch = findBranchAt(world.x, world.y)
  if (clickedBranch) {
    store.selectBranch(clickedBranch.id)
    return
  }

  isPanning.value = true
  panStart.value = { x: sx, y: sy }
  offsetStart.value = { ...viewOffset.value }
  store.clearSelection()
}

function onMouseMove(e) {
  const rect = canvasRef.value.getBoundingClientRect()
  const sx = e.clientX - rect.left
  const sy = e.clientY - rect.top
  const world = screenToWorld(sx, sy)

  hoveredBus.value = null
  hoveredBranch.value = null

  if (draggingBus.value !== null) {
    store.updateBusPosition(draggingBus.value, world.x, world.y)
    return
  }

  if (isPanning.value) {
    viewOffset.value = {
      x: offsetStart.value.x + (sx - panStart.value.x),
      y: offsetStart.value.y + (sy - panStart.value.y)
    }
    return
  }

  const bus = findBusAt(world.x, world.y)
  if (bus) {
    hoveredBus.value = bus.id
    canvasRef.value.style.cursor = 'pointer'
    return
  }

  const branch = findBranchAt(world.x, world.y)
  if (branch) {
    hoveredBranch.value = branch.id
    canvasRef.value.style.cursor = 'pointer'
    return
  }

  canvasRef.value.style.cursor = 'grab'
}

function onMouseUp() {
  isPanning.value = false
  draggingBus.value = null
}

function onWheel(e) {
  e.preventDefault()
  const rect = canvasRef.value.getBoundingClientRect()
  const sx = e.clientX - rect.left
  const sy = e.clientY - rect.top

  const worldBefore = screenToWorld(sx, sy)
  const factor = e.deltaY < 0 ? 1.1 : 0.9
  viewScale.value = Math.max(0.2, Math.min(5, viewScale.value * factor))

  const worldAfter = screenToWorld(sx, sy)
  viewOffset.value = {
    x: viewOffset.value.x + (worldAfter.x - worldBefore.x) * viewScale.value,
    y: viewOffset.value.y + (worldAfter.y - worldBefore.y) * viewScale.value
  }
}

function onDblClick() {
  resetView()
}

function findBusAt(wx, wy) {
  const threshold = NODE_RADIUS + 5
  for (const bus of store.buses) {
    const pos = getBusPos(bus.id)
    const dx = wx - pos.x
    const dy = wy - pos.y
    if (dx * dx + dy * dy <= threshold * threshold) {
      return bus
    }
  }
  return null
}

function findBranchAt(wx, wy) {
  const threshold = 10
  for (const branch of store.branches) {
    const from = getBusPos(branch.fromBus)
    const to = getBusPos(branch.toBus)

    const dx = to.x - from.x
    const dy = to.y - from.y
    const len2 = dx * dx + dy * dy
    if (len2 === 0) continue

    let t = ((wx - from.x) * dx + (wy - from.y) * dy) / len2
    t = Math.max(0, Math.min(1, t))

    const closestX = from.x + t * dx
    const closestY = from.y + t * dy
    const distX = wx - closestX
    const distY = wy - closestY

    if (distX * distX + distY * distY <= threshold * threshold) {
      return branch
    }
  }
  return null
}

function zoomIn() { viewScale.value = Math.min(5, viewScale.value * 1.2) }
function zoomOut() { viewScale.value = Math.max(0.2, viewScale.value / 1.2) }
function resetView() { viewScale.value = 1; viewOffset.value = { x: 50, y: 50 } }
function toggleFlows() { store.showFlows = !store.showFlows }
function toggleVoltages() { store.showVoltages = !store.showVoltages }

watch(
  () => [store.buses, store.branches, store.solution],
  () => {
    nextTick(() => {
      if (!containerRef.value) return
      const rect = containerRef.value.getBoundingClientRect()
      const maxX = Math.max(...store.buses.map(b => (store.busPositions[b.id]?.x || 0)))
      const maxY = Math.max(...store.buses.map(b => (store.busPositions[b.id]?.y || 0)))
      if (maxX > 0 && maxY > 0) {
        const scaleX = (rect.width - 100) / maxX
        const scaleY = (rect.height - 100) / maxY
        viewScale.value = Math.min(1, Math.min(scaleX, scaleY)) * 0.9
      }
    })
  },
  { deep: true }
)

onMounted(() => {
  initCanvas()
  window.addEventListener('resize', resizeCanvas)
})

onUnmounted(() => {
  window.removeEventListener('resize', resizeCanvas)
  if (animationId) cancelAnimationFrame(animationId)
})
</script>

<style scoped>
.grid-canvas-container {
  width: 100%;
  height: 100%;
  position: relative;
  overflow: hidden;
}

canvas {
  display: block;
  width: 100%;
  height: 100%;
  cursor: grab;
}

.canvas-controls {
  position: absolute;
  top: 16px;
  left: 16px;
  display: flex;
  flex-direction: column;
  gap: 6px;
  z-index: 10;
}

.ctrl-btn {
  width: 36px;
  height: 36px;
  border-radius: 8px;
  background-color: var(--bg-secondary);
  border: 1px solid var(--border-color);
  color: var(--text-primary);
  font-size: 16px;
  font-weight: bold;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
}

.ctrl-btn:hover {
  background-color: var(--bg-tertiary);
  border-color: var(--accent);
}

.ctrl-btn.active {
  background-color: var(--accent);
  border-color: var(--accent);
  color: white;
}

.canvas-legend {
  position: absolute;
  bottom: 16px;
  left: 16px;
  background-color: rgba(30, 41, 59, 0.9);
  border: 1px solid var(--border-color);
  border-radius: 8px;
  padding: 12px 16px;
  font-size: 12px;
  backdrop-filter: blur(8px);
  z-index: 10;
}

.legend-item {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 3px 0;
  color: var(--text-secondary);
}

.legend-dot {
  width: 14px;
  height: 14px;
  border-radius: 50%;
  border: 2px solid rgba(255, 255, 255, 0.3);
}

.legend-dot.slack { background-color: #22c55e; }
.legend-dot.pv { background-color: #f59e0b; }
.legend-dot.pq { background-color: #3b82f6; }

.legend-line {
  width: 24px;
  height: 3px;
  background-color: rgba(148, 163, 184, 0.7);
  border-radius: 2px;
}

.legend-transformer {
  width: 24px;
  height: 14px;
  position: relative;
}

.legend-transformer::before,
.legend-transformer::after {
  content: '';
  position: absolute;
  width: 10px;
  height: 10px;
  border: 2px solid #a855f7;
  border-radius: 50%;
  top: 0;
  background: rgba(168, 85, 247, 0.2);
}

.legend-transformer::before { left: 0; }
.legend-transformer::after { right: 0; }

.solution-info {
  position: absolute;
  top: 16px;
  right: 16px;
  background-color: rgba(30, 41, 59, 0.9);
  border: 1px solid var(--border-color);
  border-radius: 8px;
  padding: 12px 16px;
  font-size: 13px;
  backdrop-filter: blur(8px);
  z-index: 10;
  min-width: 180px;
}

.info-row {
  display: flex;
  justify-content: space-between;
  padding: 4px 0;
  gap: 16px;
}

.info-row span:first-child {
  color: var(--text-secondary);
}

.info-row .ok { color: var(--success); font-weight: 600; }
.info-row .err { color: var(--danger); font-weight: 600; }
</style>
