import type { Execucao, Labirinto, Tentativa } from '../../api/tipos'
import { CartaoComunicacao } from '../../components/telemetria/CartaoComunicacao'
import { BlocoHealthCheck } from '../../components/telemetria/BlocoHealthCheck'
import { MetricasTempoReal } from '../../components/telemetria/MetricasTempoReal'
import { TabelaLeituras } from '../../components/telemetria/TabelaLeituras'
import { falhasDasTentativas } from '../../components/labirinto/trajeto'
import { VisualizacaoLabirinto } from '../../components/labirinto/VisualizacaoLabirinto'

interface PainelAoVivoProps {
  execucao: Execucao
  tentativa: Tentativa
  labirinto: Labirinto
  agora: number
}

/** Telemetria da tentativa em execução (RF01, RF04). */
export function PainelAoVivo({ execucao, tentativa, labirinto, agora }: PainelAoVivoProps) {
  const celula = execucao.celula_atual ?? execucao.leituras.at(-1) ?? execucao.trajeto.at(-1)


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
              O trajeto é desenhado célula a célula na grade do labirinto escolhido. Se o lado longo estiver na vertical, o mapa
              acompanha quando o robô sair do canto da largada. Rótulos e paredes aparecem quando a tentativa terminar.
            </p>
          </section>
          <CartaoComunicacao ultimaMensagemEm={execucao.ultima_mensagem_em} />
        </div>
    </div>
  )
}
