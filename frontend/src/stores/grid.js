import { defineStore } from 'pinia'

export const useGridStore = defineStore('grid', {
  state: () => ({
    gridInfo: null,
    solution: null,
    solverRef: null,
    loading: false,
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
      maxIterations: 50
    },
    modificationHistory: []
  }),

  getters: {
    isLoaded: (state) => !!state.gridInfo && state.gridInfo.loaded,
    isSolved: (state) => !!state.solution,
    buses: (state) => state.solution?.buses || state.gridInfo?.buses || [],
    branches: (state) => state.solution?.branches || state.gridInfo?.branches || [],
    generators: (state) => state.solution?.generators || state.gridInfo?.generators || [],
    branchFlows: (state) => state.solution?.branchFlows || [],
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
          this.solverRef = result
          this.solution = null
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
      const radiusStep = radius / (total > 1 ? total - 1 : 1)

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

    async solvePowerFlow() {
      if (!this.solverRef) {
        this.error = '请先加载电网数据'
        return { success: false, error: this.error }
      }
      this.loading = true
      this.error = null
      try {
        const result = await window.electronAPI.grid.solvePowerFlow(
          this.solverRef,
          this.solverConfig
        )
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
        this.loading = false
      }
    },

    async solveWithModifications(genChanges = [], tapChanges = []) {
      if (!this.solverRef) {
        this.error = '请先加载电网数据'
        return { success: false, error: this.error }
      }
      this.loading = true
      this.error = null
      try {
        const result = await window.electronAPI.grid.solveWithModifications(
          this.solverRef,
          this.solverConfig,
          genChanges,
          tapChanges
        )
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
        this.loading = false
      }
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
      this.solverRef = null
      this.busPositions = {}
      this.selectedBus = null
      this.selectedBranch = null
      this.modificationHistory = []
    }
  }
})
