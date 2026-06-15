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

let gridSolver = null
try {
  const nativePath = path.join(process.env.APP_ROOT, '..', 'native')
  gridSolver = require(nativePath)
} catch (e) {
  console.warn('Native module not loaded:', e.message)
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
    win = null
  })
}

ipcMain.handle('grid:load-cdf', async (_event, filePath) => {
  try {
    if (!gridSolver) throw new Error('Native solver module not available')
    const solver = new gridSolver.GridSolver()
    const result = await solver.loadCDF(filePath)
    return { success: true, data: result, solverRef: solver }
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

ipcMain.handle('grid:solve-power-flow', async (_event, _solverRef, config) => {
  try {
    if (!gridSolver) throw new Error('Native solver module not available')
    if (!_solverRef || !_solverRef.data) {
      throw new Error('No grid data loaded')
    }
    const solver = new gridSolver.GridSolver()
    await solver.loadCDF(_solverRef.data.sourceFile)
    const result = await solver.solvePowerFlow(config || {})
    return { success: true, data: result }
  } catch (err) {
    return { success: false, error: err.message }
  }
})

ipcMain.handle('grid:solve-with-modifications', async (_event, _solverRef, config, genChanges, tapChanges) => {
  try {
    if (!gridSolver) throw new Error('Native solver module not available')
    if (!_solverRef || !_solverRef.data) {
      throw new Error('No grid data loaded')
    }
    const solver = new gridSolver.GridSolver()
    await solver.loadCDF(_solverRef.data.sourceFile)
    const result = await solver.solveWithModifications(config || {}, genChanges || [], tapChanges || [])
    return { success: true, data: result }
  } catch (err) {
    return { success: false, error: err.message }
  }
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
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow()
  }
})

app.whenReady().then(createWindow)
