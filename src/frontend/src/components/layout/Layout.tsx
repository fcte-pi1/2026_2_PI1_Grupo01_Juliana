import type { ReactNode } from 'react'
import { Outlet } from 'react-router'
import { BarraLateral } from './BarraLateral'
import './layout.css'

interface LayoutProps {
  /** Painel extra no rodapé (painel de cenários do mock em desenvolvimento). */
  rodape?: ReactNode
}

export function Layout({ rodape }: LayoutProps) {
  return (
    <div className="layout">
      <BarraLateral />
      <main className="layout__conteudo">
        <Outlet />
      </main>
      {rodape}
    </div>
  )
}
