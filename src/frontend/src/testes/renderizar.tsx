import { render } from '@testing-library/react'
import { createMemoryRouter, RouterProvider } from 'react-router'
import { criarRotas } from '../App'
import { definirCenario } from '../mocks/cenarioAtual'

/** Renderiza a aplicação inteira numa rota, com o cenário de mock escolhido. */
export function renderizarRota(caminho: string, cenario = '01-inicio') {
  definirCenario(cenario)
  const router = createMemoryRouter(criarRotas(), { initialEntries: [caminho] })
  return { ...render(<RouterProvider router={router} />), router }
}
