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
      '/api': {
        target: 'http://localhost:8000',
        rewrite: (caminho) => caminho.replace(/^\/api(?=\/|$)/, ''),
      },
    },
  },
  test: {
    environment: 'jsdom',
    setupFiles: ['./src/testes/setup.ts'],
    css: false,
    include: ['src/**/*.{test,spec}.?(c|m)[jt]s?(x)'],
    exclude: ['e2e/**', '**/node_modules/**', '**/.git/**'],
    // npm run test:cobertura. Fica de fora o que não vai para o build: testes, mocks do MSW e tipos gerados.
    coverage: {
      provider: 'v8',
      include: ['src/**/*.{ts,tsx}'],
      exclude: ['src/testes/**', 'src/mocks/**', 'src/main.tsx', 'src/api/schema.ts', 'src/**/*.d.ts'],
      reporter: ['text', 'text-summary', 'html'],
      thresholds: { lines: 80 },
    },
  },
})
