import { contextBridge, ipcRenderer } from 'electron'

contextBridge.exposeInMainWorld('electronAPI', {
  grid: {
    loadCDF: (filePath) => ipcRenderer.invoke('grid:load-cdf', filePath),
    solvePowerFlow: (config) => ipcRenderer.invoke('grid:solve-power-flow', config),
    solveWithModifications: (config, genChanges, tapChanges) =>
      ipcRenderer.invoke('grid:solve-with-modifications', config, genChanges, tapChanges),
    cancelComputation: () => ipcRenderer.invoke('grid:cancel-computation'),
    isComputing: () => ipcRenderer.invoke('grid:is-computing'),
    onProgress: (callback) => {
      const handler = (_event, data) => callback(data)
      ipcRenderer.on('grid:computation-progress', handler)
      return () => ipcRenderer.removeListener('grid:computation-progress', handler)
    },
    getSampleDataPath: () => ipcRenderer.invoke('app:get-sample-data-path')
  },
  dialog: {
    openFile: () => ipcRenderer.invoke('dialog:open-file')
  },
  platform: process.platform
})
