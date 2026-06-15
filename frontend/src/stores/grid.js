import { defineStore } from 'pinia'

let requestIdCounter = 0
let debounceTimer = null
const DEBOUNCE_MS = 300

export const useGridStore = defineStore('grid', {
  state: () => ({
    gridInfo: null,
    solution: null,
    loading: false,
    computing: false,
    error: null,
    busPositions: {},
    zoom: 1,
    offset: { x: 0, y: 0 },
    selectedBus: null,
    selectedBranch: null,
    showFlows: true,
    showVoltages: true,
    solverConfig: {
      tolerance: 1e-6,
      maxIterations: 50,
      timeoutMs: 30000
    },
    modificationHistory: [],
    computationProgress: null,
    lastRequestId: 0,
    pendingGenChanges: {},
    pendingTapChanges: {}
  }),

  getters: {
    isLoaded: (state) => !!state.gridInfo && state.gridInfo.loaded,
    isSolved: (state) => !!state.solution,
    buses: (state) => state.solution?.buses || state.gridInfo?.buses || [],
    branches: (state) => state.solution?.branches || state.gridInfo?.branches || [],
    generators: (state) => state.solution?.generators || state.gridInfo?.generators || [],
    branchFlows: (state) => state.solution?.branchFlows || [],
    hasPendingChanges: (state) =>
      Object.keys(state.pendingGenChanges).length > 0 || Object.keys(state.pendingTapChanges).length > 0,
    pendingChangeCount: (state) =>
      Object.keys(state.pendingGenChanges).length + Object.keys(state.pendingTapChanges).length,
    progressPercent: (state) => {
      if (!state.computationProgress) return 0
      const p = state.computationProgress
      return Math.round((p.currentIteration / p.maxIterations) * 100)
    },
    statistics: (state) => {
      if (!state.gridInfo) return null
      return {
        caseName: state.gridInfo.caseName,
        baseMVA: state.gridInfo.baseMVA,
        nBus: state.gridInfo.nBus,
        nBranch: state.gridInfo.nBranch,
        nGen: state.gridInfo.nGen,
        nPQ: state.gridInfo.nPQ,
        nPV: state.gridInfo.nPV,
        nSlack: state.gridInfo.nSlack
      }
    }
  },

  actions: {
    async loadCDFFile(filePath) {
      this.loading = true
      this.error = null
      try {
        const result = await window.electronAPI.grid.loadCDF(filePath)
        if (result.success) {
          this.gridInfo = result.data
          this.solution = null
          this.pendingGenChanges = {}
          this.pendingTapChanges = {}
          this.generateBusPositions()
        } else {
          this.error = result.error || '加载失败'
        }
        return result
      } catch (e) {
        this.error = e.message
        return { success: false, error: e.message }
      } finally {
        this.loading = false
      }
    },

    generateBusPositions() {
      const buses = this.gridInfo.buses
      const positions = {}
      const centerX = 600
      const centerY = 400
      const radius = Math.min(centerX, centerY) * 0.7

      const slackBuses = buses.filter(b => b.type === 0)
      const pvBuses = buses.filter(b => b.type === 1)
      const pqBuses = buses.filter(b => b.type === 2)
      const sortedBuses = [...slackBuses, ...pvBuses, ...pqBuses]

      const total = sortedBuses.length
      const angleStep = total > 0 ? (2 * Math.PI) / total : 0

      sortedBuses.forEach((bus, index) => {
        const angle = index * angleStep - Math.PI / 2
        const r = radius * (0.3 + 0.7 * Math.sin((index / total) * Math.PI))
        positions[bus.id] = {
          x: centerX + r * Math.cos(angle),
          y: centerY + r * Math.sin(angle)
        }
      })

      this.busPositions = positions
    },

    async cancelComputation() {
      try {
        await window.electronAPI.grid.cancelComputation()
      } catch (e) {
        console.warn('Cancel failed:', e.message)
      }
      this.computing = false
      this.computationProgress = null
    },

    async solvePowerFlow() {
      if (!this.isLoaded) {
        this.error = '请先加载电网数据'
        return { success: false, error: this.error }
      }
      if (this.computing) {
        await this.cancelComputation()
        await new Promise(r => setTimeout(r, 50))
      }

      const requestId = ++requestIdCounter
      this.lastRequestId = requestId
      this.computing = true
      this.error = null
      this.computationProgress = null

      const removeProgressListener = window.electronAPI.grid.onProgress((progress) => {
        if (requestId === this.lastRequestId) {
          this.computationProgress = progress
        }
      })

      try {
        const result = await window.electronAPI.grid.solvePowerFlow(this.solverConfig)
        if (requestId !== this.lastRequestId) {
          return { success: false, error: 'Superseded' }
        }
        if (result.success) {
          this.solution = result.data
        } else {
          this.error = result.error || '计算失败'
        }
        return result
      } catch (e) {
        this.error = e.message
        return { success: false, error: e.message }
      } finally {
        removeProgressListener()
        if (requestId === this.lastRequestId) {
          this.computing = false
          this.computationProgress = null
        }
      }
    },

    async solveWithModifications(genChanges = [], tapChanges = []) {
      if (!this.isLoaded) {
        this.error = '请先加载电网数据'
        return { success: false, error: this.error }
      }
      if (this.computing) {
        await this.cancelComputation()
        await new Promise(r => setTimeout(r, 50))
      }

      const requestId = ++requestIdCounter
      this.lastRequestId = requestId
      this.computing = true
      this.error = null
      this.computationProgress = null

      const removeProgressListener = window.electronAPI.grid.onProgress((progress) => {
        if (requestId === this.lastRequestId) {
          this.computationProgress = progress
        }
      })

      try {
        const result = await window.electronAPI.grid.solveWithModifications(
          this.solverConfig,
          genChanges,
          tapChanges
        )
        if (requestId !== this.lastRequestId) {
          return { success: false, error: 'Superseded' }
        }
        if (result.success) {
          this.solution = result.data
          this.modificationHistory.push({
            timestamp: Date.now(),
            genChanges: [...genChanges],
            tapChanges: [...tapChanges]
          })
        } else {
          this.error = result.error || '计算失败'
        }
        return result
      } catch (e) {
        this.error = e.message
        return { success: false, error: e.message }
      } finally {
        removeProgressListener()
        if (requestId === this.lastRequestId) {
          this.computing = false
          this.computationProgress = null
        }
      }
    },

    setPendingGenChange(idx, value) {
      this.pendingGenChanges = { ...this.pendingGenChanges, [idx]: value }
    },

    setPendingTapChange(idx, value) {
      this.pendingTapChanges = { ...this.pendingTapChanges, [idx]: value }
    },

    removePendingGenChange(idx) {
      const copy = { ...this.pendingGenChanges }
      delete copy[idx]
      this.pendingGenChanges = copy
    },

    removePendingTapChange(idx) {
      const copy = { ...this.pendingTapChanges }
      delete copy[idx]
      this.pendingTapChanges = copy
    },

    clearPendingChanges() {
      this.pendingGenChanges = {}
      this.pendingTapChanges = {}
    },

    debouncedSolveWithModifications(genChanges, tapChanges) {
      if (debounceTimer) clearTimeout(debounceTimer)
      return new Promise((resolve) => {
        debounceTimer = setTimeout(async () => {
          debounceTimer = null
          const result = await this.solveWithModifications(genChanges, tapChanges)
          resolve(result)
        }, DEBOUNCE_MS)
      })
    },

    updateBusPosition(busId, x, y) {
      this.busPositions[busId] = { x, y }
    },

    setZoom(zoom) {
      this.zoom = Math.max(0.2, Math.min(5, zoom))
    },

    setOffset(x, y) {
      this.offset = { x, y }
    },

    selectBus(busId) {
      this.selectedBus = busId
      this.selectedBranch = null
    },

    selectBranch(branchId) {
      this.selectedBranch = branchId
      this.selectedBus = null
    },

    clearSelection() {
      this.selectedBus = null
      this.selectedBranch = null
    },

    reset() {
      this.gridInfo = null
      this.solution = null
      this.busPositions = {}
      this.selectedBus = null
      this.selectedBranch = null
      this.modificationHistory = []
      this.pendingGenChanges = {}
      this.pendingTapChanges = {}
      this.computing = false
      this.computationProgress = null
    }
  }
})
