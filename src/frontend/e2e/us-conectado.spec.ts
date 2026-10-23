import { execFile } from 'node:child_process'
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { promisify } from 'node:util'
import { expect, test, type Page } from '@playwright/test'

/**
 * Cadeia conectada: simulador de navegação do firmware → simulador de telemetria
 * (POST /telemetria) → backend real → esta interface, sem MSW.
 *
 * Cobertura das histórias da FRONT-05:
 * - US07: tentativa aberta mostra Encerrar e 1/3; falha com tentativas restantes
 *   mostra Retomar; a terceira falha e a execução cancelada escondem a retomada.
 * - US22: motivo obrigatório, observação de no máximo 100 caracteres, um único
 *   POST no duplo clique, falha collision/stuck/out_of_track com origem do operador.
 * - US30: falha de componente na mesma execução, com nova tentativa e trajeto preservado.
 *   O firmware de navegação não emite falha_componente; essa linha vem do roteiro
 *   falha-componente.json do simulador de telemetria, depois da largada.
 * - US31: a retomada continua na mesma execução, na célula da última leitura do
 *   firmware, e o trecho seguinte do jsonl entra na tentativa nova com o prefixo intacto.
 * - Health-check: um pedido sem resultado do robô em 30 s libera nova solicitação
 *   e não abre tentativa. A aprovação anterior não atende ao pedido novo.
 */

const executar = promisify(execFile)
const API = 'http://127.0.0.1:8000'
const AQUI = path.dirname(fileURLToPath(import.meta.url))
const FIRMWARE = path.resolve(AQUI, '../../firmware/simulador-navegacao')
const SIMULADOR = path.resolve(AQUI, '../../simulador-telemetria')
const ROTEIRO_COMPONENTE = path.join(SIMULADOR, 'roteiros/falha-componente.json')

interface Mensagem {
  tipo: string
  seq: number
  x?: number
  y?: number
}

interface FalhaApi {
  motivo: string
  origem: string
  celula_x: number
  celula_y: number
  observacao: string | null
  componente: string | null
}

interface ExecucaoApi {
  execucao_id: string
  status: string
  tipo_labirinto: string
  tentativas_usadas: number
  tentativas: {
    tentativa_id: string
    attempt_index: number
    status: string
    falha: FalhaApi | null
  }[]
  trajeto: { x: number; y: number; retomada: boolean }[]
}

interface Gravacao {
  hc: string
  trecho: string
  continuacao: string
}

let pasta = ''
let gravacao: Gravacao
let proximoBoot = Math.floor(Date.now() / 1000)

function bootNovo() {
  proximoBoot += 1
  return proximoBoot
}

test.beforeAll(async () => {
  const saude = await fetch(`${API}/back-health`)
  if (!saude.ok) throw new Error(`O backend em ${API} não respondeu ao health-check.`)
  await executar('make', ['build/simulador'], { cwd: FIRMWARE })
  pasta = await mkdtemp(path.join(tmpdir(), 'us-conectado-'))
  await executar(
    './build/simulador',
    ['--telemetria', pasta, '--boot', '1', '--exigir-sucesso', 'labirintos/4x4-01-dir.txt'],
    { cwd: FIRMWARE },
  )
  const texto = await readFile(path.join(pasta, '4x4-01-dir.jsonl'), 'utf8')
  const linhas = texto.trim().split('\n').map((linha) => ({ texto: linha, json: JSON.parse(linha) as Mensagem }))
  const fimHc = linhas.findIndex((linha) => linha.json.tipo === 'hc_resultado')
  if (fimHc < 0) throw new Error('A gravação do firmware não tem hc_resultado.')
  const depois = linhas.slice(fimHc + 1)
  const fimTrecho = depois.findIndex((linha) => linha.json.tipo === 'tel' && linha.json.x === 0 && linha.json.y === 1)
  const fimContinuacao = depois.findIndex((linha) => linha.json.tipo === 'tel' && linha.json.x === 0 && linha.json.y === 2)
  if (fimTrecho < 0 || fimContinuacao < 0) throw new Error('A gravação do firmware não passou pelas células (0,1) e (0,2).')
  const hc = path.join(pasta, 'hc.jsonl')
  const trecho = path.join(pasta, 'trecho.jsonl')
  const continuacao = path.join(pasta, 'continuacao.jsonl')
  await writeFile(hc, `${linhas.slice(0, fimHc + 1).map((linha) => linha.texto).join('\n')}\n`)
  await writeFile(trecho, `${depois.slice(0, fimTrecho + 1).map((linha) => linha.texto).join('\n')}\n`)
  await writeFile(continuacao, `${depois.slice(fimTrecho + 1, fimContinuacao + 1).map((linha) => linha.texto).join('\n')}\n`)
  gravacao = { hc, trecho, continuacao }
})

test.afterAll(async () => {
  if (pasta) await rm(pasta, { recursive: true, force: true })
})

test.beforeEach(async () => {
  await cancelarExecucaoAberta()
  await confirmarInterrupcao()
  await concluirHealthCheckAberto()
})

test.afterEach(async () => {
  await cancelarExecucaoAberta()
  await confirmarInterrupcao()
})

test('US22 e US31: firmware, encerramento manual e retomada na célula da falha', async ({ page }) => {
  const boot = bootNovo()
  const batimento = pulso(boot, 0, 1)
  const observacao = 'a'.repeat(100)
  const pedidos: unknown[] = []
  page.on('request', (pedido) => {
    if (pedido.method() === 'POST' && pedido.url().includes('/encerrar-tentativa')) pedidos.push(pedido.postDataJSON())
  })
  try {
    await prepararLargada(page, boot)
    batimento.iniciar()
    await reproduzir(gravacao.trecho, boot)
    await expect(page.getByLabel('Dados da execução em tempo real').getByText('A2', { exact: true })).toBeVisible()

    await page.getByRole('button', { name: 'Encerrar tentativa', exact: true }).click()
    const modal = page.getByRole('dialog')
    const confirmar = modal.getByRole('button', { name: 'Encerrar como falha' })
    await expect(confirmar).toBeDisabled()
    await expect(modal.locator('input[type=radio]:checked')).toHaveCount(0)
    await expect(modal.getByText('Última célula A2')).toBeVisible()
    const campo = modal.getByRole('textbox')
    await campo.fill(`${observacao}b`)
    await expect(campo).toHaveValue(observacao)
    await expect(confirmar).toBeDisabled()

    await page.route('**/encerrar-tentativa', async (rota) => {
      await new Promise((resolver) => setTimeout(resolver, 600))
      await rota.continue()
    })
    await modal.getByRole('radio', { name: /collision/ }).check()
    await confirmar.evaluate((botao: HTMLButtonElement) => {
      botao.click()
      botao.click()
    })
    await expect(confirmar).toBeDisabled()
    await expect(modal.getByRole('radio', { name: /collision/ })).toBeDisabled()
    await expect(page.getByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeVisible()
    await expect(page.getByRole('button', { name: 'Encerrar execução' })).toBeVisible()
    await expect(page.getByText('Encerrada pelo operador.')).toBeVisible()
    await expect(page.getByText(`Observação: ${observacao}`)).toBeVisible()
    expect(pedidos).toEqual([{ motivo: 'collision', observacao }])

    const antes = await execucaoAtual()
    expect(antes.status).toBe('em_andamento')
    expect(antes.tipo_labirinto).toBe('4x4')
    expect(antes.tentativas_usadas).toBe(1)
    expect(antes.tentativas[0]).toMatchObject({
      attempt_index: 1,
      status: 'failed',
      falha: { motivo: 'collision', origem: 'encerrado_operador', celula_x: 0, celula_y: 1, observacao, componente: null },
    })
    expect(antes.trajeto.map((passo) => [passo.x, passo.y])).toEqual([[0, 0], [0, 1]])

    await retomar(page, 'A2')
    await expect(page.getByLabel('Tentativa 2 de 3')).toBeVisible()
    batimento.parar()
    await reproduzir(gravacao.continuacao, boot)
    await expect.poll(async () => {
      const atual = await execucaoAtual()
      return atual.trajeto.some((passo) => passo.retomada && passo.x === 0 && passo.y === 2)
    }).toBe(true)
    await page.reload()
    await expect(page.getByLabel('Tentativa 2 de 3')).toBeVisible()
    await expect(page.getByRole('button', { name: 'Encerrar tentativa', exact: true })).toBeVisible()

    const depois = await execucaoAtual()
    expect(depois.execucao_id).toBe(antes.execucao_id)
    expect(depois.status).toBe('em_andamento')
    expect(depois.tentativas_usadas).toBe(2)
    expect(depois.tentativas[1].tentativa_id).not.toBe(antes.tentativas[0].tentativa_id)
    expect(depois.tentativas[1]).toMatchObject({ attempt_index: 2, status: 'running' })
    expect(depois.tentativas[0].falha).toEqual(antes.tentativas[0].falha)
    expect(depois.trajeto.slice(0, antes.trajeto.length)).toEqual(antes.trajeto)
    expect(depois.trajeto).toContainEqual(expect.objectContaining({ x: 0, y: 2, retomada: true }))
    expect(batimento.erro()).toBeNull()
  } finally {
    batimento.parar()
  }
})

test('US07 e US22: três falhas manuais escondem a retomada', async ({ page }) => {
  const boot = bootNovo()
  const batimento = pulso(boot, 0, 0)
  try {
    await prepararLargada(page, boot)
    batimento.iniciar()
    await encerrarComMotivo(page, 'collision')
    await expect(page.getByText('Colisão com parede')).toBeVisible()
    await expect(page.getByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeVisible()
    await retomar(page, 'A1')
    await expect(page.getByLabel('Tentativa 2 de 3')).toBeVisible()

    await encerrarComMotivo(page, 'stuck')
    await expect(page.getByText('Robô travado ou parado')).toBeVisible()
    await expect(page.getByRole('button', { name: /Retomar tentativa \(3\/3\)/ })).toBeVisible()
    await retomar(page, 'A1')
    await expect(page.getByLabel('Tentativa 3 de 3')).toBeVisible()

    await encerrarComMotivo(page, 'out_of_track')
    await expect(page.getByText('Saiu do labirinto')).toBeVisible()
    await expect(page.getByRole('button', { name: /Retomar tentativa/ })).toHaveCount(0)
    await expect(page.getByText(/Sem retomada/).first()).toBeVisible()
    await expect.poll(() => new URL(page.url()).searchParams.get('execucao')).toBeTruthy()
    const atual = await execucaoNaUrl(page)
    expect(atual.status).toBe('cancelada')
    expect(atual.tentativas_usadas).toBe(3)
    expect(atual.tentativas.map((tentativa) => tentativa.falha?.motivo)).toEqual(['collision', 'stuck', 'out_of_track'])
    expect(batimento.erro()).toBeNull()
  } finally {
    batimento.parar()
  }
})

test('US07: execução cancelada não oferece retomada', async ({ page }) => {
  const boot = bootNovo()
  const batimento = pulso(boot, 0, 0)
  try {
    await prepararLargada(page, boot)
    batimento.iniciar()
    await page.getByRole('button', { name: 'Encerrar execução' }).click()
    const modal = page.getByRole('dialog')
    await modal.getByRole('button', { name: 'Encerrar execução' }).click()
    await expect(page.getByRole('heading', { name: /Execução cancelada/ })).toBeVisible()
    await expect(page.getByRole('button', { name: /Retomar tentativa/ })).toHaveCount(0)
    await expect(page.getByText(/Sem retomada/).first()).toBeVisible()
    await expect(page.getByLabel('Tentativa 1 de 3')).toBeVisible()
    expect(batimento.erro()).toBeNull()
  } finally {
    batimento.parar()
  }
})

test('US30: falha de componente permite retomar a mesma execução', async ({ page }) => {
  const boot = bootNovo()
  const batimento = pulso(boot, 0, 0)
  try {
    await prepararLargada(page, boot)
    batimento.iniciar()
    await reproduzir(ROTEIRO_COMPONENTE, bootNovo())
    batimento.parar()
    await expect(page.getByText('Falha de componente')).toBeVisible()
    await expect(page.getByText(/Componente: Motor direito/)).toBeVisible()
    await expect(page.getByText('Detectada automaticamente.')).toBeVisible()
    await expect(page.getByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeVisible()

    const antes = await execucaoAtual()
    expect(antes.status).toBe('em_andamento')
    expect(antes.tentativas_usadas).toBe(1)
    expect(antes.tentativas[0].falha).toMatchObject({
      motivo: 'falha_componente',
      origem: 'automatica',
      componente: 'motor_direito',
      celula_x: 0,
      celula_y: 3,
    })
    expect(antes.trajeto.map((passo) => [passo.x, passo.y])).toEqual([[0, 0], [0, 1], [0, 2], [0, 3]])

    await retomar(page, 'A4')
    await page.reload()
    await expect(page.getByLabel('Tentativa 2 de 3')).toBeVisible()
    await expect(page.getByRole('button', { name: 'Encerrar tentativa', exact: true })).toBeVisible()
    const depois = await execucaoAtual()
    expect(depois.execucao_id).toBe(antes.execucao_id)
    expect(depois.tentativas_usadas).toBe(2)
    expect(depois.tentativas[1].tentativa_id).not.toBe(antes.tentativas[0].tentativa_id)
    expect(depois.tentativas[1]).toMatchObject({ attempt_index: 2, status: 'running' })
    expect(depois.tentativas[0].falha).toEqual(antes.tentativas[0].falha)
    expect(depois.trajeto.slice(0, antes.trajeto.length)).toEqual(antes.trajeto)
    expect(batimento.erro()).toBeNull()
  } finally {
    batimento.parar()
  }
})

test('health-check: 30 s sem o robô libera nova solicitação e não abre tentativa', async ({ page }) => {
  await page.goto('/')
  const solicitar = page.getByRole('button', { name: 'Solicitar health-check', exact: true })
  const iniciar = page.getByRole('button', { name: /Nova execução/ })
  await expect(solicitar).toBeEnabled()
  await solicitar.click()
  await expect(page.getByText('Aguardando o health-check do robô. Nenhuma tentativa foi aberta.')).toBeVisible()
  await expect(iniciar).toBeDisabled()
  await expect(page.getByLabel('Tentativa 1 de 3')).toHaveCount(0)

  await expect(page.getByRole('alert')).toContainText('não concluiu o health-check em 30 s', { timeout: 40_000 })
  await expect(page.getByText('A verificação não foi concluída. Solicite um novo health-check.')).toBeVisible()
  await expect(solicitar).toBeEnabled()
  await expect(iniciar).toBeDisabled()
  expect((await fetch(`${API}/execucoes/em-andamento`)).status).toBe(204)

  await solicitar.click()
  await expect(page.getByText('Aguardando o health-check do robô. Nenhuma tentativa foi aberta.')).toBeVisible()
  await expect(page.getByRole('alert')).toHaveCount(0)
  await expect(solicitar).toBeDisabled()
  await expect(iniciar).toBeDisabled()
})

async function prepararLargada(page: Page, boot: number) {
  await page.goto('/')
  const solicitar = page.getByRole('button', { name: 'Solicitar health-check', exact: true })
  await expect(solicitar).toBeEnabled()
  await solicitar.click()
  await expect(page.getByText('Aguardando o health-check do robô. Nenhuma tentativa foi aberta.')).toBeVisible()
  await expect(page.getByRole('button', { name: /Nova execução/ })).toBeDisabled()
  await expect(page.getByLabel('Tentativa 1 de 3')).toHaveCount(0)
  const aberta = await fetch(`${API}/execucoes/em-andamento`)
  expect(aberta.status).toBe(204)

  await reproduzir(gravacao.hc, boot)
  const iniciar = page.getByRole('button', { name: /Nova execução/ })
  await expect(iniciar).toBeEnabled()
  await expect(page.getByText('Health-check aprovado e válido para abrir a tentativa.')).toBeVisible()
  await page.getByText('Ver componentes do health-check').click()
  await expect(page.getByText('8 de 8 OK')).toBeVisible()
  await expect(page.getByRole('listitem').filter({ hasText: 'ToF frontal' })).toContainText('OK')

  await iniciar.click()
  await expect(page.getByLabel('Tentativa 1 de 3')).toBeVisible()
  await expect(page.getByRole('button', { name: 'Encerrar tentativa', exact: true })).toBeVisible()
  await expect(page.getByRole('button', { name: /Retomar tentativa/ })).toHaveCount(0)
}

async function encerrarComMotivo(page: Page, motivo: string) {
  await page.getByRole('button', { name: 'Encerrar tentativa', exact: true }).click()
  const modal = page.getByRole('dialog')
  await modal.getByRole('radio', { name: new RegExp(motivo) }).check()
  await modal.getByRole('button', { name: 'Encerrar como falha' }).click()
  await expect(modal).toHaveCount(0)
}

async function retomar(page: Page, celula: string) {
  await page.getByRole('button', { name: /Retomar tentativa \(\d\/3\)/ }).click()
  const modal = page.getByRole('dialog')
  const confirmar = modal.getByRole('button', { name: `Retomar da célula ${celula}`, exact: true })
  await expect(confirmar).toBeDisabled()
  await modal.getByRole('checkbox').nth(0).check()
  await expect(confirmar).toBeDisabled()
  await modal.getByRole('checkbox').nth(1).check()
  await expect(confirmar).toBeEnabled()
  await confirmar.click()
  await expect(page.getByRole('button', { name: 'Encerrar tentativa', exact: true })).toBeVisible()
}

async function reproduzir(arquivo: string, boot: number) {
  // Um `interromper` ainda pendente faz o simulador cortar o arquivo na primeira tel.
  await confirmarInterrupcao()
  const resultado = await executar(
    'uv',
    ['run', 'python', '-m', 'simulador', arquivo, '--saida', 'http', '--url', API, '--sem-espera', '--boot', String(boot)],
    { cwd: SIMULADOR, encoding: 'utf8', maxBuffer: 10 * 1024 * 1024, timeout: 60_000 },
  )
  if (resultado.stderr.includes('recusou a linha') || resultado.stderr.includes('não foram enviadas')) {
    throw new Error(resultado.stderr)
  }
}

function pulso(boot: number, x: number, y: number) {
  let seq = 100_000
  let tMs = 100_000
  let ativo = false
  let ultimoErro: string | null = null
  let timer: ReturnType<typeof setInterval> | null = null

  async function enviar() {
    if (!ativo) return
    seq += 1
    tMs += 200
    const linha = JSON.stringify({
      v: 1, boot, seq, t_ms: tMs, tipo: 'tel', estado: 'running', x, y, rumo: 'N', bat_mv: 8000, vel_mm_s: 0, eixo_longo: null,
    })
    try {
      const resposta = await fetch(`${API}/telemetria`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ linha, recebido_em: new Date().toISOString() }),
      })
      if (!resposta.ok && ativo) ultimoErro = `${resposta.status} ${await resposta.text()}`
    } catch (erro) {
      if (ativo) ultimoErro = erro instanceof Error ? erro.message : String(erro)
    }
  }

  return {
    iniciar() {
      ativo = true
      void enviar()
      timer = setInterval(() => { void enviar() }, 2_000)
    },
    parar() {
      ativo = false
      if (timer) clearInterval(timer)
      timer = null
    },
    erro: () => ultimoErro,
  }
}

async function execucaoAtual() {
  const resposta = await fetch(`${API}/execucoes/em-andamento`)
  if (!resposta.ok) throw new Error(`GET /execucoes/em-andamento respondeu ${resposta.status}.`)
  return normalizar(await resposta.json() as ExecucaoApi)
}

async function execucaoNaUrl(page: Page) {
  const id = new URL(page.url()).searchParams.get('execucao')
  if (!id) throw new Error('A página não guardou o id da execução na URL.')
  const resposta = await fetch(`${API}/execucoes/${id}`)
  if (!resposta.ok) throw new Error(`GET /execucoes/${id} respondeu ${resposta.status}.`)
  return normalizar(await resposta.json() as ExecucaoApi)
}

function normalizar(corpo: ExecucaoApi) {
  corpo.tentativas.sort((a, b) => a.attempt_index - b.attempt_index)
  return corpo
}

async function confirmarInterrupcao() {
  const linha = JSON.stringify({
    v: 1, boot: bootNovo(), seq: 0, t_ms: 0, tipo: 'falha', motivo: null, origem: 'web', x: 0, y: 0, componente: null,
  })
  const resposta = await fetch(`${API}/telemetria`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ linha, recebido_em: new Date().toISOString() }),
  })
  if (!resposta.ok) throw new Error(`A confirmação da interrupção foi recusada: ${resposta.status} ${await resposta.text()}`)
}

async function concluirHealthCheckAberto() {
  const resposta = await fetch(`${API}/health-check/ultimo`)
  if (resposta.status === 204) return
  if (!resposta.ok) throw new Error(`GET /health-check/ultimo respondeu ${resposta.status}.`)
  const corpo = await resposta.json() as { aprovado: boolean | null }
  if (corpo.aprovado == null) await reproduzir(gravacao.hc, bootNovo())
}

async function cancelarExecucaoAberta() {
  const resposta = await fetch(`${API}/execucoes/em-andamento`)
  if (resposta.status === 204) return
  if (!resposta.ok) throw new Error(`GET /execucoes/em-andamento respondeu ${resposta.status}.`)
  const corpo = await resposta.json() as { execucao_id: string }
  const fim = await fetch(`${API}/execucoes/${corpo.execucao_id}/encerrar`, { method: 'POST' })
  if (!fim.ok && fim.status !== 409) throw new Error(await fim.text())
}
