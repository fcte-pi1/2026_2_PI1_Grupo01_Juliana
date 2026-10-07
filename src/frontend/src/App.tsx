import type { ReactNode } from 'react'
import type { RouteObject } from 'react-router'
import { Layout } from './components/layout/Layout'
import { DetalheExecucao } from './pages/DetalheExecucao'
import { Historico } from './pages/Historico'
import { Inicio } from './pages/inicio/Inicio'
import { NaoEncontrada } from './pages/NaoEncontrada'

/**
 * Rotas da aplicação. Os modais de Encerrar e Retomar tentativa ficam na rota `/`
 * e podem ser abertos direto pela URL (`/?modal=encerrar`, `/?modal=retomar`).
 */
export function criarRotas(rodape?: ReactNode): RouteObject[] {
  return [
    {
      element: <Layout rodape={rodape} />,
      children: [
        { path: '/', element: <Inicio /> },
        { path: '/execucoes', element: <Historico /> },
        { path: '/execucoes/:id', element: <DetalheExecucao /> },
        { path: '*', element: <NaoEncontrada /> },
      ],
    },
  ]
}
