import { setupServer } from 'msw/node'
import { handlers } from './handlers'

/** Mesmos handlers do navegador, usados nos testes (Vitest). */
export const servidor = setupServer(...handlers)
