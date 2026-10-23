import type { TipoLabirinto } from '../../api/tipos'
import type { useHealthCheck } from '../../hooks/useHealthCheck'
import { ROTULO_COMPONENTE } from '../../utils/formatacao'
import { BlocoHealthCheck } from './BlocoHealthCheck'

interface Props {
  verificacao: ReturnType<typeof useHealthCheck>
  tipo: TipoLabirinto
  bloqueado?: boolean
  tentativaAberta?: boolean
}

export function VerificacaoHealthCheck({ verificacao: hc, tipo, bloqueado = false, tentativaAberta = false }: Props) {
  const mensagem = hc.carregando ? 'Consultando o health-check…'
    : hc.tempoEsgotado ? 'A verificação não foi concluída. Solicite um novo health-check.'
      : hc.verificando ? 'Aguardando o health-check do robô. Nenhuma tentativa foi aberta.'
        : !hc.dados ? 'Realize o health-check antes de iniciar ou retomar.'
          : hc.dados.aprovado === false ? 'Health-check reprovado. Corrija a falha e solicite outra verificação.'
            : !hc.valido ? 'Health-check sem validade vigente. Solicite uma nova verificação.'
              : hc.dados.tipo_dip !== tipo ? `O robô informou ${hc.dados.tipo_dip ?? 'tipo pendente'}; o labirinto selecionado é ${tipo}.`
                : 'Health-check aprovado e válido para abrir a tentativa.'
  return (
    <section className="cartao" aria-label="Verificação antes da tentativa">
      <h2 className="cartao__titulo">Health-check antes da largada</h2>
      <p role="status" className="texto-suave">{mensagem}</p>
      {hc.erro && <p role="alert" className="texto-perigo">{hc.erro}</p>}
      <button type="button" className="botao espaco-acima" disabled={bloqueado || hc.solicitando || hc.verificando}
        onClick={() => { void hc.solicitar() }}>
        {hc.solicitando ? 'Solicitando…' : 'Solicitar health-check'}
      </button>
      {tentativaAberta && <p className="texto-suave">Encerre a tentativa aberta antes de solicitar outro health-check.</p>}
      {hc.dados && <details className="espaco-acima" open={hc.verificando}>
        <summary>Ver componentes do health-check</summary>
        <BlocoHealthCheck verificando={hc.verificando} itens={Object.keys(ROTULO_COMPONENTE).map((componente) => {
          const item = hc.dados!.itens?.find((item) => item.componente === componente)
          return { componente: componente as keyof typeof ROTULO_COMPONENTE, aprovado: item?.aprovado ?? null, valor_lido: item?.valor_lido ?? null }
        })} />
      </details>}
    </section>
  )
}
