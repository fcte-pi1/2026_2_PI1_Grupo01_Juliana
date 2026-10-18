import type { Execucao, Labirinto, Tentativa } from '../../api/tipos'
import { CartaoFalha } from '../../components/execucao/CartaoFalha'
import { falhasDasTentativas } from '../../components/labirinto/trajeto'
import { VisualizacaoLabirinto } from '../../components/labirinto/VisualizacaoLabirinto'
import { CartaoMetrica } from '../../components/telemetria/CartaoMetrica'
import { BlocoHealthCheck } from '../../components/telemetria/BlocoHealthCheck'
import { MetricasTempoReal } from '../../components/telemetria/MetricasTempoReal'
import { TabelaLeituras } from '../../components/telemetria/TabelaLeituras'
import { formatarMedicao, formatarNumero, formatarTempo, LIMITE_TENTATIVAS, LADO_CELULA_M, nomeCelula } from '../../utils/formatacao'

interface PainelEncerradaProps {
  execucao: Execucao
  tentativa: Tentativa
  labirinto: Labirinto
  agora: number
  aoRetomar?: () => void
}

/** Tentativa encerrada (sucesso ou falha): trajeto completo e métricas finais (RF01, RF09, RF15). */
export function PainelEncerrada({ execucao, tentativa, labirinto, agora, aoRetomar }: PainelEncerradaProps) {
  const { falha } = tentativa
  const podeRetomar = tentativa.status === 'failed' && execucao.status === 'em_andamento' && execucao.tentativas_usadas < LIMITE_TENTATIVAS

  return (
    <div className="painel-tempo-real painel-tempo-real--encerrada pagina-duas-colunas">
        <div className="coluna">
          <section className="cartao">
            <div className="cartao__cabecalho">
              <div>
                <h2 className="cartao__titulo">Trajeto percorrido</h2>
                <p className="texto-suave">Montado com as {execucao.trajeto.length} células gravadas, em ordem</p>
              </div>
              <span className="mono texto-suave">
                {formatarNumero(Math.max(execucao.trajeto.length - 1, 0) * LADO_CELULA_M, 2)} m
              </span>
            </div>
            <VisualizacaoLabirinto
              largura={labirinto.largura}
              altura={labirinto.altura}
              eixoLongo={execucao.eixo_longo}
              trajeto={execucao.trajeto}
              celulaAtual={execucao.celula_atual ?? execucao.leituras.at(-1) ?? execucao.trajeto.at(-1) ?? null}
              posicaoAtual={execucao.celula_atual}
              falhas={falhasDasTentativas(execucao.tentativas)}
            />
          </section>
          {execucao.leituras.length > 0 && <TabelaLeituras leituras={execucao.leituras} largadaEm={tentativa.iniciada_em} />}
          <BlocoHealthCheck itens={tentativa.health_check} />
        </div>

        <div className="coluna">
          <MetricasTempoReal execucao={execucao} tentativa={tentativa} agora={agora} compacta>
            <CartaoMetrica
              rotulo="Bateria usada"
              valor={tentativa.consumo_bateria !== null ? formatarMedicao(tentativa.consumo_bateria) : '—'}
              unidade="mV"
              detalhe={tentativa.consumo_bateria_pct == null ? 'Percentual pendente' : `${formatarMedicao(tentativa.consumo_bateria_pct)} p.p. de carga`}
            />
          </MetricasTempoReal>
          {execucao.status === 'concluida' && <CartaoMetrica
              rotulo="Variação contra o melhor tempo"
              valor={execucao.variacao_melhor_tempo_pct == null ? '—' : formatarMedicao(execucao.variacao_melhor_tempo_pct)}
              unidade="%"
              detalhe="em relação ao melhor tempo anterior"
            />}
          {falha && <CartaoFalha falha={falha} attemptIndex={tentativa.attempt_index} />}
          {execucao.status !== 'concluida' && <p className="texto-suave">Sem comparação de tempo: o robô não chegou ao objetivo.</p>}
          {(falha || execucao.tentativas.length > 1) && (
            <section className={`cartao${podeRetomar ? ' cartao--retomada' : ''}`}>
              <h2 className="cartao__titulo">{podeRetomar ? 'Retomar tentativa' : execucao.status === 'cancelada' ? 'Sem retomada disponível' : 'Tentativas desta execução'}</h2>
              <ul className="tentativas-resumo">
                {execucao.tentativas.map((item) => (
                  <li key={item.tentativa_id}>
                    <span>Tentativa {item.attempt_index}/{LIMITE_TENTATIVAS}{item.falha && ` · ${nomeCelula({ x: item.falha.celula_x, y: item.falha.celula_y })}`}</span>
                    <strong className="mono">{item.tempo_s == null ? '—' : formatarTempo(item.tempo_s)}</strong>
                  </li>
                ))}
              </ul>
              {podeRetomar && falha && <>
                <p className="texto-suave espaco-acima">A tentativa {tentativa.attempt_index + 1}/{LIMITE_TENTATIVAS} começa na célula da falha {nomeCelula({ x: falha.celula_x, y: falha.celula_y })}. O trajeto já gravado é mantido.</p>
                {aoRetomar && <button type="button" className="botao botao--retomada espaco-acima" onClick={aoRetomar}>Retomar da célula {nomeCelula({ x: falha.celula_x, y: falha.celula_y })} →</button>}
              </>}
              {execucao.status === 'cancelada' && <p className="texto-suave espaco-acima">Crie uma nova execução para partir de A1.</p>}
            </section>
          )}
        </div>
    </div>
  )
}
