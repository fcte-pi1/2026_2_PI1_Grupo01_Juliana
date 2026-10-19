import { Link, useParams } from 'react-router'
import { ErroApi } from '../api/clienteApi'
import { CartaoFalha } from '../components/execucao/CartaoFalha'
import { PilulaResultado, PilulaStatusTentativa } from '../components/execucao/PilulaResultado'
import { Cabecalho } from '../components/layout/Cabecalho'
import { falhasDasTentativas } from '../components/labirinto/trajeto'
import { VisualizacaoLabirinto } from '../components/labirinto/VisualizacaoLabirinto'
import { BlocoHealthCheck } from '../components/telemetria/BlocoHealthCheck'
import { CartaoMetrica } from '../components/telemetria/CartaoMetrica'
import { useExecucao } from '../hooks/useExecucao'
import { formatarDataHora, formatarMedicao, formatarNumeroExecucao, formatarTempo, LIMITE_TENTATIVAS } from '../utils/formatacao'
import { tentativaAtual } from './inicio/estadoInicio'

/** Rota `/execucoes/:id`: detalhe da execução com trajeto e tentativas (FRONT-06, RF02, RF05). */
export function DetalheExecucao() {
  const { id = '' } = useParams()
  const { dados: execucao, erro } = useExecucao(id)

  if (erro) {
    const naoExiste = erro instanceof ErroApi && erro.status === 404
    return (
      <>
        <Cabecalho trilha="Histórico de execuções" titulo={naoExiste ? 'Execução não encontrada' : 'Erro ao carregar'} />
        <div className="conteudo">
          <p className="estado-vazio">
            {naoExiste ? 'Essa execução não existe.' : erro.message} <Link to="/execucoes">Voltar ao histórico</Link>
          </p>
        </div>
      </>
    )
  }
  if (!execucao) return <p className="estado-vazio">Carregando…</p>

  const [largura, altura] = execucao.labirinto.split('x').map(Number)
  const ultima = tentativaAtual(execucao)

  return (
    <>
      <Cabecalho
        trilha="Histórico de execuções · detalhe"
        titulo={`Execução ${formatarNumeroExecucao(execucao.numero, execucao.execucao_id)}`}
        acoes={
          <>
            <PilulaResultado resultado={execucao.status} />
            <Link to="/execucoes" className="botao">
              Voltar ao histórico
            </Link>
          </>
        }
      />
      <div className="conteudo">
        <section className="cartao resumo-execucao">
          <span className="chip">Labirinto {execucao.labirinto}</span>
          <span className="chip">
            {execucao.tentativas_usadas}/{LIMITE_TENTATIVAS} tentativas
          </span>
          <span className="chip">Criada {formatarDataHora(execucao.iniciada_em)}</span>
          <span className="chip">Tempo total {execucao.tempo_total_s !== null ? formatarTempo(execucao.tempo_total_s) : '—'}</span>
        </section>

        <CartaoMetrica
          rotulo="Variação contra o melhor tempo"
          valor={execucao.variacao_melhor_tempo_pct == null ? '—' : formatarMedicao(execucao.variacao_melhor_tempo_pct)}
          unidade="%"
          detalhe="em relação ao melhor tempo anterior"
        />

        <section className="cartao">
          <h2 className="cartao__titulo">Trajeto</h2>
          <VisualizacaoLabirinto
            largura={largura}
            altura={altura}
            eixoLongo={execucao.eixo_longo}
            trajeto={execucao.trajeto}
            posicaoAtual={execucao.celula_atual}
            falhas={falhasDasTentativas(execucao.tentativas)}
          />
        </section>

        <section className="cartao">
          <h2 className="cartao__titulo">Tentativas em ordem cronológica</h2>
          <div className="rolagem-horizontal">
            <table className="tabela tabela--sem-destaque">
              <thead>
                <tr>
                  <th>#</th>
                  <th>Status</th>
                  <th className="alinhar-direita">Tempo</th>
                  <th className="alinhar-direita">Veloc.</th>
                  <th className="alinhar-direita">Consumo</th>
                  <th className="alinhar-direita">Carga usada (p.p.)</th>
                </tr>
              </thead>
              <tbody>
                {execucao.tentativas.map((t) => (
                  <tr key={t.tentativa_id}>
                    <td>
                      {t.attempt_index}/{LIMITE_TENTATIVAS}
                    </td>
                    <td>
                      <PilulaStatusTentativa status={t.status} />
                    </td>
                    <td className="alinhar-direita">{t.tempo_s !== null ? formatarTempo(t.tempo_s) : '—'}</td>
                    <td className="alinhar-direita">
                      {t.velocidade_media !== null ? `${formatarMedicao(t.velocidade_media)} m/s` : '—'}
                    </td>
                    <td className="alinhar-direita">{t.consumo_bateria == null ? '—' : `${formatarMedicao(t.consumo_bateria)} mV`}</td>
                    <td className="alinhar-direita">{t.consumo_bateria_pct == null ? '—' : `${formatarMedicao(t.consumo_bateria_pct)} p.p.`}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>

        {execucao.tentativas
          .filter((t) => t.falha)
          .map((t) => (
            <CartaoFalha key={t.tentativa_id} falha={t.falha!} attemptIndex={t.attempt_index} />
          ))}
        {ultima && ultima.health_check.length > 0 && <BlocoHealthCheck itens={ultima.health_check} />}
      </div>
    </>
  )
}
