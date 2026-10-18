import type { Execucao, Labirinto, Tentativa } from '../../api/tipos'
import { CartaoComunicacao } from '../../components/telemetria/CartaoComunicacao'
import { BlocoHealthCheck } from '../../components/telemetria/BlocoHealthCheck'
import { MetricasTempoReal } from '../../components/telemetria/MetricasTempoReal'
import { TabelaLeituras } from '../../components/telemetria/TabelaLeituras'
import { falhasDasTentativas } from '../../components/labirinto/trajeto'
import { VisualizacaoLabirinto } from '../../components/labirinto/VisualizacaoLabirinto'
import { formatarMedicao, nomeCelula, ROTULO_COMPONENTE } from '../../utils/formatacao'

interface PainelAoVivoProps {
  execucao: Execucao
  tentativa: Tentativa
  labirinto: Labirinto
  agora: number
}

/** Telemetria da tentativa aberta: health-check ou em execução (RF01, RF04). */
export function PainelAoVivo({ execucao, tentativa, labirinto, agora }: PainelAoVivoProps) {
  const celula = execucao.celula_atual ?? execucao.leituras.at(-1) ?? execucao.trajeto.at(-1)

  if (tentativa.status === 'health-check') {
    return (
      <div className="painel-tempo-real pagina-duas-colunas">
        <div className="coluna">
          <BlocoHealthCheck itens={tentativa.health_check} verificando />
          <MetricasTempoReal execucao={execucao} tentativa={tentativa} agora={agora} />
        </div>
        <div className="coluna">
          <section className="cartao">
            <h2 className="cartao__titulo">Sensores na {tentativa.tipo_inicio === 'retomada' ? 'retomada' : 'largada'}{celula ? ` (${nomeCelula(celula)})` : ''}</h2>
            <p className="texto-suave">Leituras do health-check enviadas pelo robô.</p>
            <div className="sensores-diagrama" aria-hidden="true">
              <span className="sensores-diagrama__frontal">frontal</span>
              <span className="sensores-diagrama__esquerdo">esquerdo</span>
              <div className="sensores-diagrama__robo"><i /><i /><i /></div>
              <span className="sensores-diagrama__direito">direito</span>
            </div>
            <dl className="lista-dados">
              {(['tof_frontal', 'tof_esquerdo', 'tof_direito'] as const).map((componente) => {
                const item = tentativa.health_check.find((item) => item.componente === componente)
                return <div key={componente}><dt>{ROTULO_COMPONENTE[componente]}</dt><dd className="mono">{item?.valor_lido == null ? '—' : `${formatarMedicao(item.valor_lido)} mm`}</dd></div>
              })}
            </dl>
            <p className="texto-suave espaco-acima">Os resultados do health-check ficam separados dos dados do percurso.</p>
          </section>
          <CartaoComunicacao ultimaMensagemEm={execucao.ultima_mensagem_em} />
        </div>
      </div>
    )
  }

  return (
    <div className="painel-tempo-real pagina-duas-colunas">
        <div className="coluna">
          <MetricasTempoReal execucao={execucao} tentativa={tentativa} agora={agora} />
          {execucao.leituras.length > 0 && <TabelaLeituras leituras={execucao.leituras} largadaEm={tentativa.iniciada_em} />}
          <BlocoHealthCheck itens={tentativa.health_check} />
        </div>

        <div className="coluna">
          <section className="cartao trajeto-ao-vivo">
            <h2 className="cartao__titulo">Trajeto</h2>
            <VisualizacaoLabirinto
              largura={labirinto.largura}
              altura={labirinto.altura}
              eixoLongo={execucao.eixo_longo}
              trajeto={execucao.trajeto}
              celulaAtual={celula ?? null}
              posicaoAtual={execucao.celula_atual}
              falhas={falhasDasTentativas(execucao.tentativas)}
              compacto
            />
            <p className="texto-suave espaco-acima">
              O trajeto é desenhado célula a célula conforme o robô entra em cada uma. No 8x4 e no 12x4, o mapa mostra só o 4x4 da
              largada até o robô revelar a orientação; o mapa com rótulos e paredes aparece quando a tentativa terminar.
            </p>
          </section>
          <CartaoComunicacao ultimaMensagemEm={execucao.ultima_mensagem_em} />
        </div>
    </div>
  )
}
