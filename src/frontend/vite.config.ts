/// <reference types="vitest/config" />
import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

export default defineConfig({
  plugins: [react()],
  // Os mocks entram por import dinâmico; sem isto o Vite reotimiza e recarrega a página no primeiro acesso.
  optimizeDeps: {
    include: ['msw', 'msw/browser'],
  },
  server: {
    // Sem mock, /api vai para o backend FastAPI local (docker compose).
    proxy: {
      '/api': 'http://localhost:8000',
    },
  },
  test: {
    environment: 'jsdom',
    setupFiles: ['./src/testes/setup.ts'],
    css: false,
  },
})
