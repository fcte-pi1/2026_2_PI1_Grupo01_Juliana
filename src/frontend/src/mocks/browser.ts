import { setupWorker } from 'msw/browser'
import { criarHandlerStream, handlers } from './handlers'

export const worker = setupWorker(...handlers, criarHandlerStream())
