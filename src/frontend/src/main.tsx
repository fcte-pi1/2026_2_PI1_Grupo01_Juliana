import { StrictMode, type ReactNode } from 'react'
import { createRoot } from 'react-dom/client'
import { createBrowserRouter, RouterProvider } from 'react-router'
import { criarRotas } from './App'
import './styles/global.css'
import './styles/componentes.css'

async function iniciar() {
  let rodape: ReactNode = null

  // Fora do modo mock, o Vite remove este bloco do build.
  if (import.meta.env.VITE_API_MOCK === 'true') {
    const { worker } = await import('./mocks/browser')
    const { PainelCenarios } = await import('./mocks/PainelCenarios')
    await worker.start({ onUnhandledRequest: 'bypass' })
    rodape = <PainelCenarios />
  }

  const router = createBrowserRouter(criarRotas(rodape))
  createRoot(document.getElementById('root')!).render(
    <StrictMode>
      <RouterProvider router={router} />
    </StrictMode>,
  )
}

void iniciar()
