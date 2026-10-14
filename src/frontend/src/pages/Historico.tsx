import { useSearchParams } from 'react-router'
import type { TipoLabirinto } from '../api/tipos'
import { FiltroLabirinto } from '../components/historico/FiltroLabirinto'
import { TabelaExecucoes } from '../components/historico/TabelaExecucoes'
import { Cabecalho } from '../components/layout/Cabecalho'
import { useExecucoes } from '../hooks/useExecucoes'

const TIPOS: TipoLabirinto[] = ['4x4', '8x4', '12x4']

/** Rota `/execucoes`: histórico com filtro por labirinto (FRONT-06, RF03). */
export function Historico() {
  const [parametros, setParametros] = useSearchParams()
  const filtroUrl = parametros.get('labirinto') as TipoLabirinto | null
  const filtro = filtroUrl && TIPOS.includes(filtroUrl) ? filtroUrl : null
  const { dados, erro, carregando } = useExecucoes(filtro)

  return (
    <>
      <Cabecalho trilha="Monitoramento" titulo="Histórico de execuções" />
      <div className="conteudo">
        <section className="cartao">
          <div className="cartao__cabecalho">
            <h2 className="cartao__titulo">Execuções, da mais recente para a mais antiga</h2>
            <FiltroLabirinto valor={filtro} aoMudar={(valor) => setParametros(valor ? { labirinto: valor } : {})} />
          </div>
          {erro && <p className="estado-vazio">Não foi possível carregar o histórico: {erro.message}</p>}
          {!erro && carregando && !dados && <p className="estado-vazio">Carregando…</p>}
          {dados && <TabelaExecucoes execucoes={dados} />}
        </section>
      </div>
    </>
  )
}
