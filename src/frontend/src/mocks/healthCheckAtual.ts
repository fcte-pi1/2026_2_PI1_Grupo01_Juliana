import type { TipoLabirinto, UltimoHealthCheck, MotivoRecusa } from '../api/tipos'
import { healthCheck } from './dados'

const CHAVE = 'mock:health-check:v1'
const VALIDADE_MS = 420_000 // Mesmo padrão da configuração do backend; somente demonstração.
type Estado = { dados: UltimoHealthCheck; concluido: UltimoHealthCheck | null; base: number; inicio: number }
let estado: Estado | null = null

export function limparHealthCheckMock() {
  estado = null
  sessionStorage.removeItem(CHAVE)
}

function salvar() { sessionStorage.setItem(CHAVE, JSON.stringify(estado)) }

/** Reutiliza os oito resultados já definidos nos exemplos do projeto. */
export function ultimoHealthCheckMock(agora: number, cenario: string): UltimoHealthCheck {
  if (!estado) {
    try { estado = JSON.parse(sessionStorage.getItem(CHAVE) ?? 'null') as Estado | null } catch { /* sessão ausente */ }
    if (!estado) {
      const verificando = cenario === '02-health-check'
      const dados: UltimoHealthCheck = { iniciado_em: new Date(agora).toISOString(), tipo_dip: '4x4',
        aprovado: verificando ? null : true, concluido_em: verificando ? null : new Date(agora).toISOString(),
        valido_ate: verificando ? null : new Date(agora + VALIDADE_MS).toISOString(),
        itens: healthCheck(verificando ? 6 : 8).filter((item) => item.aprovado !== null)
          .map((item) => ({ ...item, aprovado: item.aprovado! })) }
      estado = { dados, concluido: verificando ? null : dados, base: verificando ? 6 : 8, inicio: agora }
    }
  }
  if (estado.dados.aprovado === null) {
    const total = Math.min(8, estado.base + Math.floor((agora - estado.inicio) / 1_000))
    estado.dados = { ...estado.dados, itens: healthCheck(total).filter((item) => item.aprovado !== null)
      .map((item) => ({ ...item, aprovado: item.aprovado! })) }
    if (total === 8) {
      estado.dados = { ...estado.dados, aprovado: true, concluido_em: new Date(agora).toISOString(),
        valido_ate: new Date(agora + VALIDADE_MS).toISOString() }
      estado.concluido = estado.dados
    }
  }
  salvar()
  return estado.dados
}

export function solicitarHealthCheckMock(agora: number, cenario: string) {
  const anterior = ultimoHealthCheckMock(agora, cenario)
  estado = { dados: { ...anterior, iniciado_em: new Date(agora).toISOString(), aprovado: null,
    concluido_em: null, valido_ate: null, itens: [] }, concluido: estado!.concluido, inicio: agora, base: 0 }
  salvar()
  return { solicitado_em: new Date(agora).toISOString() }
}

export function recusaHealthCheckMock(tipo: TipoLabirinto, agora: number, cenario: string): MotivoRecusa | null {
  ultimoHealthCheckMock(agora, cenario)
  const concluido = estado!.concluido
  if (!concluido) return 'health_check_ausente'
  if (!concluido.aprovado) return 'health_check_reprovado'
  if (!concluido.valido_ate || Date.parse(concluido.valido_ate) < agora) return 'health_check_expirado'
  if (concluido.tipo_dip !== tipo) return 'dip_divergente'
  return null
}
