import { contextBridge, ipcRenderer } from 'electron'

contextBridge.exposeInMainWorld('electronAPI', {
  grid: {
    loadCDF: (filePath) => ipcRenderer.invoke('grid:load-cdf', filePath),
    solvePowerFlow: (solverRef, config) => ipcRenderer.invoke('grid:solve-power-flow', solverRef, config),
    solveWithModifications: (solverRef, config, genChanges, tapChanges) =>
      ipcRenderer.invoke('grid:solve-with-modifications', solverRef, config, genChanges, tapChanges),
    getSampleDataPath: () => ipcRenderer.invoke('app:get-sample-data-path')
  },
  dialog: {
    openFile: () => ipcRenderer.invoke('dialog:open-file')
  },
  platform: process.platform
})
