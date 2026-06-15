import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import electron from 'vite-plugin-electron/simple'
import path from 'path'

export default defineConfig({
  plugins: [
    vue(),
    electron({
      main: {
        entry: 'electron/main/index.js',
        vite: {
          build: {
            outDir: 'dist-electron/main'
          }
        }
      },
      preload: {
        input: path.join(__dirname, 'electron/preload/index.js'),
        vite: {
          build: {
            outDir: 'dist-electron/preload'
          }
        }
      },
      renderer: process.env.NODE_ENV === 'test' ? undefined : {}
    })
  ],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, 'src')
    }
  },
  server: {
    port: 5173,
    strictPort: true
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true
  }
})
