/// <reference types="node" />
import { defineConfig, devices } from '@playwright/test'

/** Frontend real, sem MSW, contra o backend em http://127.0.0.1:8000. */
export default defineConfig({
  testDir: './e2e',
  testMatch: 'us-conectado.spec.ts',
  fullyParallel: false,
  workers: 1,
  forbidOnly: Boolean(process.env.CI),
  retries: 0,
  timeout: 180_000,
  expect: { timeout: 20_000 },
  reporter: process.env.CI ? 'github' : 'list',
  use: {
    baseURL: 'http://127.0.0.1:4176',
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
  },
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    },
  ],
  webServer: {
    command: 'npm run dev -- --host 127.0.0.1 --port 4176',
    url: 'http://127.0.0.1:4176',
    reuseExistingServer: false,
    timeout: 30_000,
    env: {
      ...Object.fromEntries(Object.entries(process.env).filter((entrada): entrada is [string, string] => entrada[1] !== undefined)),
      VITE_API_MOCK: 'false',
    },
  },
})
