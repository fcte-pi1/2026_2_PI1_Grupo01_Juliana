import { setupWorker } from 'msw/browser'
import { criarHandlerHealthCheck, criarHandlerStream, handlers } from './handlers'

export const worker = setupWorker(...handlers, criarHandlerStream(), criarHandlerHealthCheck())
