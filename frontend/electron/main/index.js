import { app, BrowserWindow, ipcMain, dialog } from 'electron'
import { fileURLToPath } from 'node:url'
import path from 'node:path'
import fs from 'node:fs'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

process.env.APP_ROOT = path.join(__dirname, '../..')
export const VITE_DEV_SERVER_URL = process.env.VITE_DEV_SERVER_URL
export const MAIN_DIST = path.join(process.env.APP_ROOT, 'dist-electron')
export const RENDERER_DIST = path.join(process.env.APP_ROOT, 'dist')
process.env.VITE_PUBLIC = VITE_DEV_SERVER_URL
  ? path.join(process.env.APP_ROOT, 'public')
  : RENDERER_DIST

let win = null
let activeSolver = null

let GridSolverClass = null
try {
  const nativePath = path.join(process.env.APP_ROOT, '..', 'native')
  const nativeModule = require(nativePath)
  GridSolverClass = nativeModule.GridSolver
} catch (e) {
  console.warn('Native module not loaded:', e.message)
}

const DEFAULT_TIMEOUT_MS = 30000
let computationTimeout = null

function clearComputationTimeout() {
  if (computationTimeout) {
    clearTimeout(computationTimeout)
    computationTimeout = null
  }
}

function cancelActiveComputation() {
  clearComputationTimeout()
  if (activeSolver) {
    try {
      activeSolver.cancelComputation()
    } catch (e) {
      console.warn('Error cancelling computation:', e.message)
    }
  }
}

function createWindow() {
  win = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 1000,
    minHeight: 700,
    title: 'Smart Grid Simulator - 电网潮流计算系统',
    icon: path.join(process.env.VITE_PUBLIC, 'favicon.ico'),
    webPreferences: {
      preload: path.join(__dirname, '../preload/index.mjs'),
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: false
    }
  })

  if (VITE_DEV_SERVER_URL) {
    win.loadURL(VITE_DEV_SERVER_URL)
    win.webContents.openDevTools()
  } else {
    win.loadFile(path.join(RENDERER_DIST, 'index.html'))
  }

  win.on('closed', () => {
    cancelActiveComputation()
    activeSolver = null
    win = null
  })
}

ipcMain.handle('grid:load-cdf', async (_event, filePath) => {
  try {
    if (!GridSolverClass) throw new Error('Native solver module not available')
    cancelActiveComputation()
    activeSolver = new GridSolverClass()
    const result = activeSolver.loadCDF(filePath)
    return { success: true, data: result }
  } catch (err) {
    return { success: false, error: err.message }
  }
})

ipcMain.handle('dialog:open-file', async () => {
  const result = await dialog.showOpenDialog(win, {
    properties: ['openFile'],
    filters: [
      { name: 'IEEE CDF Files', extensions: ['cdf', 'txt', 'dat'] },
      { name: 'All Files', extensions: ['*'] }
    ]
  })
  if (!result.canceled && result.filePaths.length > 0) {
    return { success: true, filePath: result.filePaths[0] }
  }
  return { success: false, canceled: true }
})

ipcMain.handle('grid:solve-power-flow', async (_event, config) => {
  try {
    if (!GridSolverClass) throw new Error('Native solver module not available')
    if (!activeSolver || !activeSolver.isGridLoaded()) {
      throw new Error('No grid data loaded')
    }
    if (activeSolver.isComputing()) {
      cancelActiveComputation()
      await new Promise(resolve => setTimeout(resolve, 50))
    }

    const timeoutMs = config?.timeoutMs || DEFAULT_TIMEOUT_MS

    const result = await new Promise((resolve, reject) => {
      clearComputationTimeout()
      computationTimeout = setTimeout(() => {
        cancelActiveComputation()
        reject(new Error(`Computation timeout (${timeoutMs}ms)`))
      }, timeoutMs + 5000)

      activeSolver.solvePowerFlow(config || {}, (progress) => {
        if (win && !win.isDestroyed()) {
          win.webContents.send('grid:computation-progress', progress)
        }
      }).then((data) => {
        clearComputationTimeout()
        resolve(data)
      }).catch((err) => {
        clearComputationTimeout()
        reject(err)
      })
    })

    return { success: true, data: result }
  } catch (err) {
    return { success: false, error: err.message }
  }
})

ipcMain.handle('grid:solve-with-modifications', async (_event, config, genChanges, tapChanges) => {
  try {
    if (!GridSolverClass) throw new Error('Native solver module not available')
    if (!activeSolver || !activeSolver.isGridLoaded()) {
      throw new Error('No grid data loaded')
    }
    if (activeSolver.isComputing()) {
      cancelActiveComputation()
      await new Promise(resolve => setTimeout(resolve, 50))
    }

    const timeoutMs = config?.timeoutMs || DEFAULT_TIMEOUT_MS

    const result = await new Promise((resolve, reject) => {
      clearComputationTimeout()
      computationTimeout = setTimeout(() => {
        cancelActiveComputation()
        reject(new Error(`Computation timeout (${timeoutMs}ms)`))
      }, timeoutMs + 5000)

      activeSolver.solveWithModifications(
        config || {},
        genChanges || [],
        tapChanges || [],
        (progress) => {
          if (win && !win.isDestroyed()) {
            win.webContents.send('grid:computation-progress', progress)
          }
        }
      ).then((data) => {
        clearComputationTimeout()
        resolve(data)
      }).catch((err) => {
        clearComputationTimeout()
        reject(err)
      })
    })

    return { success: true, data: result }
  } catch (err) {
    return { success: false, error: err.message }
  }
})

ipcMain.handle('grid:cancel-computation', async () => {
  cancelActiveComputation()
  return { success: true }
})

ipcMain.handle('grid:is-computing', async () => {
  if (!activeSolver) return { success: true, computing: false }
  return { success: true, computing: activeSolver.isComputing() }
})

ipcMain.handle('app:get-sample-data-path', async () => {
  const sampleDir = path.join(process.env.APP_ROOT, '..', 'samples')
  if (fs.existsSync(sampleDir)) {
    const files = fs.readdirSync(sampleDir).filter(f => f.endsWith('.cdf') || f.endsWith('.txt'))
    return {
      success: true,
      directory: sampleDir,
      files: files.map(f => ({ name: f, path: path.join(sampleDir, f) }))
    }
  }
  return { success: false, error: 'Sample directory not found' }
})

app.on('window-all-closed', () => {
  cancelActiveComputation()
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('before-quit', () => {
  cancelActiveComputation()
})

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow()
  }
})

app.whenReady().then(createWindow)
