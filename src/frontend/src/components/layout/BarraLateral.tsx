import { NavLink } from 'react-router'
import { StatusReceptor } from './StatusReceptor'

const DOCUMENTACAO = 'https://fcte-pi1.github.io/2026_2_PI1_Grupo01_Juliana/'
const REPOSITORIO = 'https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana'

export function BarraLateral() {
  return (
    <aside className="lateral">
      <div className="lateral__marca">
        <span className="lateral__logo" aria-hidden="true" />
        <div>
          <strong>Ratatouille</strong>
          <span className="lateral__sub mono">telemetria · grupo 01</span>
        </div>
      </div>

      <nav className="lateral__nav" aria-label="Principal">
        <span className="lateral__secao rotulo">Monitoramento</span>
        <NavLink to="/" end className="lateral__item">
          Início
        </NavLink>
        <NavLink to="/execucoes" className="lateral__item">
          Histórico de execuções
        </NavLink>

        <span className="lateral__secao rotulo">Projeto</span>
        <a href={DOCUMENTACAO} target="_blank" rel="noreferrer" className="lateral__item">
          Documentação <span aria-hidden="true">↗</span>
        </a>
        <a href={REPOSITORIO} target="_blank" rel="noreferrer" className="lateral__item">
          Repositório <span aria-hidden="true">↗</span>
        </a>
      </nav>

      {/* Estado real do receptor vem do backend no BACK-08. */}
      <StatusReceptor estado="desconhecido" />
      <span className="lateral__versao mono">v0.1 · PI1 2026.2</span>
    </aside>
  )
}
