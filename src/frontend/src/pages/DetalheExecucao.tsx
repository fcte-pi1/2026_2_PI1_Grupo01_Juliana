import { Link, useParams } from 'react-router'
import { ErroApi } from '../api/clienteApi'
import { CartaoFalha } from '../components/execucao/CartaoFalha'
import { PilulaResultado, PilulaStatusTentativa } from '../components/execucao/PilulaResultado'
import { Cabecalho } from '../components/layout/Cabecalho'
import { VisualizacaoLabirinto } from '../components/labirinto/VisualizacaoLabirinto'
import { BlocoHealthCheck } from '../components/telemetria/BlocoHealthCheck'
import { useExecucao } from '../hooks/useExecucao'
import { formatarDataHora, formatarNumero, formatarNumeroExecucao, formatarTempo, LIMITE_TENTATIVAS } from '../utils/formatacao'
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
  const falha = ultima?.falha

  return (
    <>
      <Cabecalho
        trilha="Histórico de execuções · detalhe"
        titulo={`Execução ${formatarNumeroExecucao(execucao.numero)}`}
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

        <div className="pagina-duas-colunas">
          <section className="cartao">
            <h2 className="cartao__titulo">Trajeto</h2>
            <VisualizacaoLabirinto
              largura={largura}
              altura={altura}
              trajeto={execucao.trajeto}
              celulaFalha={falha ? { x: falha.celula_x, y: falha.celula_y } : null}
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
                        {t.velocidade_media !== null ? `${formatarNumero(t.velocidade_media, 3)} m/s` : '—'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </section>
        </div>

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
