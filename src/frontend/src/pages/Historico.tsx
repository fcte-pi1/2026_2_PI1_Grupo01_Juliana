import { useSearchParams } from 'react-router'
import type { TipoLabirinto } from '../api/tipos'
import { CartaoCampanha } from '../components/historico/CartaoCampanha'
import { FiltroConfiguracao } from '../components/historico/FiltroConfiguracao'
import { FiltroLabirinto } from '../components/historico/FiltroLabirinto'
import { TabelaExecucoes } from '../components/historico/TabelaExecucoes'
import { Cabecalho } from '../components/layout/Cabecalho'
import { useCampanha } from '../hooks/useCampanha'
import { useExecucoes } from '../hooks/useExecucoes'

const TIPOS: TipoLabirinto[] = ['4x4', '8x4', '12x4']

/** Rota `/execucoes`: histórico com filtro por labirinto e por configuração de paredes (FRONT-06, RF03). */
export function Historico() {
  const [parametros, setParametros] = useSearchParams()
  const filtroUrl = parametros.get('labirinto') as TipoLabirinto | null
  const filtro = filtroUrl && TIPOS.includes(filtroUrl) ? filtroUrl : null
  const configuracao = parametros.get('configuracao')
  const { dados, erro, carregando } = useExecucoes(filtro, configuracao)
  const campanha = useCampanha(filtro, configuracao)

  const mudar = (novo: { labirinto?: TipoLabirinto | null; configuracao?: string | null }) => {
    const proximo = new URLSearchParams()
    const labirinto = 'labirinto' in novo ? novo.labirinto : filtro
    const conf = 'configuracao' in novo ? novo.configuracao : configuracao
    if (labirinto) proximo.set('labirinto', labirinto)
    if (conf) proximo.set('configuracao', conf)
    setParametros(proximo)
  }
  // As opções vêm do resumo sem o filtro de configuração, senão o filtro escolhido apagaria as outras.
  const todasConfiguracoes = useCampanha(filtro, null)
  const configuracoes = (todasConfiguracoes.dados?.por_configuracao ?? [])
    .map((grupo) => grupo.configuracao)
    .filter((nome): nome is string => nome !== null)

  return (
    <>
      <Cabecalho trilha="Monitoramento" titulo="Histórico de execuções" />
      <div className="conteudo">
        {campanha.dados && <CartaoCampanha campanha={campanha.dados} labirinto={filtro} configuracao={configuracao} />}
        <section className="cartao">
          <div className="cartao__cabecalho">
            <h2 className="cartao__titulo">Execuções, da mais recente para a mais antiga</h2>
            <div className="filtros-historico">
              <FiltroConfiguracao configuracoes={configuracoes} valor={configuracao} aoMudar={(valor) => mudar({ configuracao: valor })} />
              <FiltroLabirinto valor={filtro} aoMudar={(valor) => mudar({ labirinto: valor })} />
            </div>
          </div>
          {erro && <p className="estado-vazio">Não foi possível carregar o histórico: {erro.message}</p>}
          {!erro && carregando && !dados && <p className="estado-vazio">Carregando…</p>}
          {dados && <TabelaExecucoes execucoes={dados} />}
        </section>
      </div>
    </>
  )
}
