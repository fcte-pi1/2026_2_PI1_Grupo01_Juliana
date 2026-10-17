import { ROTULO_COMPONENTE, ROTULO_MOTIVO_FALHA } from '../utils/formatacao'
import type { ComponenteHealthCheck, EventoExecucao, Execucao, PassoTrajeto, Tentativa } from './tipos'

const STATUS_EXECUCAO = ['em_andamento', 'concluida', 'cancelada']
const STATUS_TENTATIVA = ['health-check', 'running', 'success', 'failed']

/** Ignora mensagens inválidas sem interromper a conexão nem substituir os últimos dados. */
export function lerEvento(tipo: EventoExecucao['tipo'], texto: string): EventoExecucao | null {
  let dados: Record<string, unknown>
  try {
    const valor: unknown = JSON.parse(texto)
    if (!valor || typeof valor !== 'object' || Array.isArray(valor)) return null
    dados = valor as Record<string, unknown>
  } catch {
    return null
  }
  if (dados.tipo !== tipo) return null
  const numeros = (...campos: string[]) => campos.every((campo) => typeof dados[campo] === 'number' && Number.isFinite(dados[campo]))
  const textoValido = (campo: string) => typeof dados[campo] === 'string' && dados[campo].length > 0
  const dataValida = (campo: string) => textoValido(campo) && Number.isFinite(Date.parse(dados[campo] as string))
  const execucaoValida = () => STATUS_EXECUCAO.includes(String(dados.status_execucao))
  const numeroOpcional = (campo: string) => dados[campo] == null || numeros(campo)
  let valido = false
  switch (tipo) {
    case 'telemetria':
      valido = numeros('ordem', 'x', 'y', 'bateria') && dataValida('enviado_em') && numeroOpcional('velocidade')
      break
    case 'passo':
      valido = numeros('passo_id', 'seq', 'x', 'y', 'paredes_mask') && typeof dados.retomada === 'boolean'
        && (dados.entrou_em == null || dataValida('entrou_em'))
      break
    case 'status':
      valido = textoValido('tentativa_id') && numeros('attempt_index') && execucaoValida()
        && Number.isInteger(dados.attempt_index) && Number(dados.attempt_index) >= 1 && Number(dados.attempt_index) <= 3
        && STATUS_TENTATIVA.includes(String(dados.status_tentativa))
        && (dados.tipo_dip == null || ['4x4', '8x4', '12x4', 'invalido'].includes(String(dados.tipo_dip)))
      break
    case 'health_check':
      valido = textoValido('tentativa_id') && Object.hasOwn(ROTULO_COMPONENTE, String(dados.componente))
        && typeof dados.aprovado === 'boolean' && numeroOpcional('valor_lido')
      break
    case 'falha':
      valido = textoValido('tentativa_id') && execucaoValida() && numeros('celula_x', 'celula_y')
        && Object.hasOwn(ROTULO_MOTIVO_FALHA, String(dados.motivo))
        && ['automatica', 'encerrado_operador'].includes(String(dados.origem)) && dataValida('momento_falha')
        && (dados.componente == null || Object.hasOwn(ROTULO_COMPONENTE, String(dados.componente)))
        && (dados.observacao == null || typeof dados.observacao === 'string')
      break
    case 'sucesso':
      valido = textoValido('tentativa_id') && execucaoValida() && numeros('x', 'y')
  }
  return valido ? dados as EventoExecucao : null
}

export function ultimaTentativa(execucao: Execucao): Tentativa | undefined {
  return execucao.tentativas.reduce<Tentativa | undefined>((atual, tentativa) =>
    !atual || tentativa.attempt_index > atual.attempt_index ? tentativa : atual, undefined)
}

function encerrarOuAtualizar(tentativa: Tentativa, evento: Exclude<EventoExecucao, { tipo: 'telemetria' | 'passo' }>): Tentativa {
  if (tentativa.tentativa_id !== evento.tentativa_id) return tentativa
  switch (evento.tipo) {
    case 'status': {
      const finalizada = tentativa.status === 'success' || tentativa.status === 'failed'
      if (finalizada && (evento.status_tentativa === 'running' || evento.status_tentativa === 'health-check')) return tentativa
      return { ...tentativa, status: evento.status_tentativa, tipo_descoberto: evento.tipo_dip === undefined ? tentativa.tipo_descoberto : evento.tipo_dip }
    }
    case 'health_check': {
      const itens = new Map(tentativa.health_check.map((item) => [item.componente, item]))
      itens.set(evento.componente, { componente: evento.componente, aprovado: evento.aprovado, valor_lido: evento.valor_lido ?? null })
      return { ...tentativa, health_check: (Object.keys(ROTULO_COMPONENTE) as ComponenteHealthCheck[]).map((componente) =>
        itens.get(componente) ?? { componente, aprovado: null, valor_lido: null }) }
    }
    case 'falha':
      return { ...tentativa, status: 'failed', encerrada_em: tentativa.encerrada_em ?? evento.momento_falha,
        falha: { motivo: evento.motivo, origem: evento.origem, celula_x: evento.celula_x, celula_y: evento.celula_y,
          componente: evento.componente ?? null, observacao: evento.observacao ?? null, momento_falha: evento.momento_falha } }
    case 'sucesso':
      return { ...tentativa, status: 'success' }
  }
}

function unirPassos(anteriores: PassoTrajeto[], novos: PassoTrajeto[]): PassoTrajeto[] {
  const chave = (passo: PassoTrajeto) => passo.passo_id === undefined ? `seq:${passo.seq}` : `id:${passo.passo_id}`
  const passos = new Map(anteriores.map((passo) => [chave(passo), passo]))
  for (const passo of novos) passos.set(chave(passo), passo)
  return [...passos.values()].sort((a, b) => (a.passo_id ?? a.seq) - (b.passo_id ?? b.seq))
}

/** Aplica somente informações enviadas pelo servidor; datas e métricas finais vêm do GET. */
export function aplicarEvento(execucao: Execucao, evento: EventoExecucao, recebidoEm?: number): Execucao {
  const atualizada = (dados: Execucao): Execucao => recebidoEm === undefined ? dados : { ...dados, ultima_atualizacao_em: recebidoEm }
  if (evento.tipo === 'telemetria') {
    const tentativa = ultimaTentativa(execucao)
    if (execucao.status !== 'em_andamento' || tentativa?.status === 'success' || tentativa?.status === 'failed') return execucao
    if (tentativa && Date.parse(evento.enviado_em) < Date.parse(tentativa.iniciada_em)) return execucao
    const ultima = execucao.leituras.at(-1)
    if (ultima && (evento.ordem <= ultima.ordem || Date.parse(evento.enviado_em) < Date.parse(ultima.enviado_em))) return execucao
    return atualizada({ ...execucao, ultima_mensagem_em: evento.enviado_em, celula_atual: { x: evento.x, y: evento.y },
      leituras: [...execucao.leituras.slice(-4), { ordem: evento.ordem, x: evento.x, y: evento.y, bateria: evento.bateria,
        velocidade: evento.velocidade ?? null, enviado_em: evento.enviado_em }] })
  }
  if (evento.tipo === 'passo') {
    if (execucao.trajeto.some((passo) => passo.passo_id === evento.passo_id)) return execucao
    const ultimoId = Math.max(0, ...execucao.trajeto.map((passo) => passo.passo_id ?? 0))
    const completa = { ...execucao, trajeto: unirPassos(execucao.trajeto, [evento]) }
    const tentativa = ultimaTentativa(execucao)
    const encerrada = execucao.status !== 'em_andamento' || tentativa?.status === 'success' || tentativa?.status === 'failed'
    const anterior = evento.entrou_em && tentativa && Date.parse(evento.entrou_em) < Date.parse(tentativa.iniciada_em)
    return evento.passo_id > ultimoId && !encerrada && !anterior
      ? atualizada({ ...completa, celula_atual: { x: evento.x, y: evento.y } }) : completa
  }
  const tentativa = execucao.tentativas.find((item) => item.tentativa_id === evento.tentativa_id)
  if (!tentativa) return execucao // O hook consulta o GET antes de aplicar uma tentativa ainda desconhecida.
  const atual = ultimaTentativa(execucao)
  if (atual && tentativa.attempt_index < atual.attempt_index) {
    return { ...execucao, tentativas: execucao.tentativas.map((item) => encerrarOuAtualizar(item, evento)) }
  }
  if (execucao.status !== 'em_andamento' && 'status_execucao' in evento && evento.status_execucao === 'em_andamento') return execucao
  if (evento.tipo === 'health_check') {
    const item = tentativa?.health_check.find((item) => item.componente === evento.componente)
    if (item?.aprovado === evento.aprovado && item.valor_lido === (evento.valor_lido ?? null)) return execucao
  }
  if (evento.tipo === 'status' && execucao.status === evento.status_execucao && tentativa?.status === evento.status_tentativa
    && (evento.tipo_dip === undefined || tentativa.tipo_descoberto === evento.tipo_dip)) return execucao
  return atualizada({ ...execucao,
    status: 'status_execucao' in evento ? evento.status_execucao : execucao.status,
    celula_atual: evento.tipo === 'sucesso' ? { x: evento.x, y: evento.y }
      : evento.tipo === 'falha' ? { x: evento.celula_x, y: evento.celula_y } : execucao.celula_atual,
    tentativas: execucao.tentativas.map((tentativa) => encerrarOuAtualizar(tentativa, evento)),
  })
}

/** Mantém leituras conhecidas se a consulta ainda não as trouxe, sem misturar tentativas. */
export function combinarRetrato(atual: Execucao, retrato: Execucao): Execucao {
  if (atual.execucao_id !== retrato.execucao_id) return retrato
  const mesmaTentativa = ultimaTentativa(atual)?.tentativa_id === ultimaTentativa(retrato)?.tentativa_id
  const amostras = new Map(mesmaTentativa ? atual.leituras.map((leitura) => [leitura.ordem, leitura]) : [])
  for (const leitura of retrato.leituras) {
    const anterior = amostras.get(leitura.ordem)
    if (!anterior || Date.parse(leitura.enviado_em) >= Date.parse(anterior.enviado_em)) amostras.set(leitura.ordem, leitura)
  }
  const leituras = [...amostras.values()].sort((a, b) => a.ordem - b.ordem).slice(-5)
  const atualMaisRecente = mesmaTentativa && (atual.leituras.at(-1)?.ordem ?? -1) >= (retrato.leituras.at(-1)?.ordem ?? -1)
  const novoPasso = (retrato.trajeto.at(-1)?.passo_id ?? -1) > (atual.trajeto.at(-1)?.passo_id ?? -1)
  const tentativa = ultimaTentativa(retrato)
  const falha = tentativa?.status === 'failed' ? tentativa.falha : null
  const horarios = (mesmaTentativa ? [atual.ultima_mensagem_em, retrato.ultima_mensagem_em] : [retrato.ultima_mensagem_em])
    .filter((horario): horario is string => horario !== null)
    .sort((a, b) => Date.parse(b) - Date.parse(a))
  return { ...retrato,
    trajeto: unirPassos(atual.trajeto, retrato.trajeto),
    leituras,
    celula_atual: falha ? { x: falha.celula_x, y: falha.celula_y }
      : atualMaisRecente && !novoPasso ? atual.celula_atual ?? retrato.celula_atual : retrato.celula_atual,
    ultima_atualizacao_em: mesmaTentativa ? atual.ultima_atualizacao_em : undefined,
    ultima_mensagem_em: horarios[0] ?? leituras.at(-1)?.enviado_em ?? null,
  }
}
