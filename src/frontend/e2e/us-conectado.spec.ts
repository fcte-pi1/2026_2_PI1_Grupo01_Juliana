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
 * - FRONT-06, com a mesma gravação do 4x4: US03 lista da mais recente para a
 *   mais antiga; US26 e US27 filtram 4x4, 8x4, 12x4 e Todos; US02 mostra as
 *   três tentativas em ordem, com tempo, velocidade e consumo; US16 mostra a
 *   variação que o backend calculou; US28 reexibe o trajeto completo depois
 *   de duas interrupções do operador e do sucesso na terceira.
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
  resto: string
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
  const resto = path.join(pasta, 'resto.jsonl')
  await writeFile(hc, `${linhas.slice(0, fimHc + 1).map((linha) => linha.texto).join('\n')}\n`)
  await writeFile(trecho, `${depois.slice(0, fimTrecho + 1).map((linha) => linha.texto).join('\n')}\n`)
  await writeFile(continuacao, `${depois.slice(fimTrecho + 1, fimContinuacao + 1).map((linha) => linha.texto).join('\n')}\n`)
  await writeFile(resto, `${depois.slice(fimTrecho + 1).map((linha) => linha.texto).join('\n')}\n`)
  gravacao = { hc, trecho, continuacao, resto }
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

test('US02, US03, US16 e US26 a US28: histórico, filtro e as três tentativas do 4x4', async ({ page }) => {
  const fatias = fatiarCorrida(await readFile(path.join(pasta, '4x4-01-dir.jsonl'), 'utf8'))
  const arquivos = await Promise.all(fatias.trechos.map(async (conteudo, indice) => {
    const arquivo = path.join(pasta, `us28-t${indice + 1}.jsonl`)
    await writeFile(arquivo, conteudo)
    return arquivo
  }))

  await prepararLargada(page, bootNovo())
  const id = (await execucaoAtual()).execucao_id
  const rotulo = await rotuloDaExecucao(id)

  await reproduzir(arquivos[0], bootNovo())
  await expect(page.getByLabel('Dados da execução em tempo real').getByText(fatias.paradas[0], { exact: true })).toBeVisible()
  await encerrarComMotivo(page, 'collision')
  await expect(page.getByText('Colisão com parede')).toBeVisible()
  await expect(page.getByText('Encerrada pelo operador.')).toBeVisible()
  await retomar(page, fatias.paradas[0])

  await reproduzir(arquivos[1], bootNovo())
  await expect(page.getByLabel('Dados da execução em tempo real').getByText(fatias.paradas[1], { exact: true })).toBeVisible()
  await encerrarComMotivo(page, 'stuck')
  await expect(page.getByText('Robô travado ou parado')).toBeVisible()
  await retomar(page, fatias.paradas[1])

  await reproduzir(arquivos[2], bootNovo())
  await expect.poll(async () => (await detalhe(id)).status).toBe('concluida')
  const pronto = await detalhe(id)
  expect(pronto.tipo_labirinto).toBe('4x4')
  expect(pronto.tentativas.map((tentativa) => tentativa.status)).toEqual(['failed', 'failed', 'success'])
  expect(pronto.tentativas[0].falha).toMatchObject({
    motivo: 'collision', origem: 'encerrado_operador', componente: null,
    celula_x: letra(fatias.paradas[0]), celula_y: numeroCelula(fatias.paradas[0]),
  })
  expect(pronto.tentativas[1].falha).toMatchObject({
    motivo: 'stuck', origem: 'encerrado_operador', componente: null,
    celula_x: letra(fatias.paradas[1]), celula_y: numeroCelula(fatias.paradas[1]),
  })
  expect(pronto.tentativas[2].falha).toBeNull()
  expect(pronto.trajeto.at(-1)).toMatchObject({ x: letra(fatias.objetivo), y: numeroCelula(fatias.objetivo) })

  await page.goto('/execucoes')
  await conferirLista(page, null)
  await expect(page.getByRole('table').getByRole('link').first()).toHaveText(rotulo)

  const filtro = page.getByRole('group', { name: 'Filtrar por labirinto' })
  await filtro.getByRole('button', { name: '4x4' }).click()
  await conferirLista(page, '4x4')
  await expect(page.getByRole('link', { name: rotulo })).toBeVisible()
  await filtro.getByRole('button', { name: '8x4' }).click()
  await conferirLista(page, '8x4')
  await expect(page.getByRole('link', { name: rotulo })).toHaveCount(0)
  await filtro.getByRole('button', { name: '12x4' }).click()
  await conferirLista(page, '12x4')
  await expect(page.getByRole('link', { name: rotulo })).toHaveCount(0)
  await filtro.getByRole('button', { name: 'Todos' }).click()
  await conferirLista(page, null)

  await page.goto(`/execucoes/${id}`)
  await expect(page.getByRole('heading', { name: `Execução ${rotulo}` })).toBeVisible()
  const variacao = page.locator('section').filter({ has: page.getByText('Variação contra o melhor tempo') })
  await expect(variacao).toContainText(pronto.variacao_melhor_tempo_pct == null ? '—' : String(pronto.variacao_melhor_tempo_pct).replace('.', ','))
  const linhas = page.getByRole('row')
  for (const tentativa of pronto.tentativas) {
    const linha = linhas.nth(tentativa.attempt_index)
    await expect(linha).toContainText(`${tentativa.attempt_index}/3`)
    await expect(linha).toContainText(tentativa.status === 'success' ? 'Sucesso' : 'Falha')
    await expect(linha).toContainText(formatarTempo(tentativa.tempo_s))
    await expect(linha).toContainText(formatarMedicao(tentativa.velocidade_media, 'm/s'))
    await expect(linha).toContainText(formatarMedicao(tentativa.consumo_bateria, 'mV'))
  }
  const falha1 = page.locator('section').filter({ has: page.getByRole('heading', { name: 'Registro da falha · tentativa 1' }) })
  await expect(falha1).toContainText(fatias.paradas[0])
  await expect(falha1).toContainText('Colisão com parede')
  await expect(falha1).toContainText('Encerrada pelo operador')
  await expect(falha1).not.toContainText('Componente:')
  const falha2 = page.locator('section').filter({ has: page.getByRole('heading', { name: 'Registro da falha · tentativa 2' }) })
  await expect(falha2).toContainText(fatias.paradas[1])
  await expect(falha2).toContainText('Robô travado ou parado')
  await expect(page.getByRole('heading', { name: 'Registro da falha · tentativa 3' })).toHaveCount(0)

  const visitadas = new Set(pronto.trajeto.map((passo) => `${passo.x},${passo.y}`)).size
  await expect(page.getByRole('img', { name: `Labirinto 4x4 com ${visitadas} células visitadas` })).toBeVisible()
  await expect.poll(() => page.locator('svg.labirinto__svg title').allTextContents()).toEqual(
    expect.arrayContaining([expect.stringContaining(`${fatias.objetivo} ·`)]),
  )
  await expect(page.getByText('OBJETIVO', { exact: true })).toBeVisible()
  expect(await page.evaluate(caminhoLivre)).toBe(true)

  const grupo = page.getByRole('group', { name: 'Trajeto por tentativa' })
  await grupo.getByRole('button', { name: 'T1' }).click()
  await expect(page.locator('.labirinto__robo title')).toHaveText(`Robô em ${fatias.paradas[0]} · falhou`)
  await expect(page.getByText('FALHA T2')).toBeVisible()
  await grupo.getByRole('button', { name: 'T2' }).click()
  await expect(page.getByText(`Retomada em ${fatias.paradas[0]}`)).toBeVisible()
  await expect(page.locator('.labirinto__robo title')).toHaveText(`Robô em ${fatias.paradas[1]} · falhou`)
  await grupo.getByRole('button', { name: 'T3' }).click()
  await expect(page.getByText(`Retomada em ${fatias.paradas[1]}`)).toBeVisible()
  await expect(page.locator('.labirinto__robo title')).toHaveText(`Robô em ${fatias.objetivo}`)
  await expect(page.getByText('OBJETIVO', { exact: true })).toBeVisible()
  expect(await page.evaluate(caminhoLivre)).toBe(true)
})

test('E2E-01: execução completa com sucesso, do health-check à conclusão', async ({ page }) => {
  const boot = bootNovo()
  await prepararLargada(page, boot)
  const id = (await execucaoAtual()).execucao_id

  await reproduzir(gravacao.trecho, boot)
  await expect(page.getByLabel('Tentativa 1 de 3')).toBeVisible()
  await expect(page.getByLabel('Dados da execução em tempo real').getByText('A2', { exact: true })).toBeVisible()
  expect((await execucaoAtual()).trajeto.map((passo) => [passo.x, passo.y])).toEqual([[0, 0], [0, 1]])

  await reproduzir(gravacao.resto, boot)
  await expect.poll(async () => (await detalhe(id)).status).toBe('concluida')
  const pronto = await detalhe(id)
  expect(pronto.tentativas).toHaveLength(1)
  expect(pronto.tentativas[0]).toMatchObject({ attempt_index: 1, status: 'success', falha: null })
  expect(pronto.tentativas[0].tempo_s).toBeGreaterThan(0)
  expect(pronto.tentativas[0].velocidade_media).toBeGreaterThan(0)
  expect(pronto.trajeto[0]).toMatchObject({ x: 0, y: 0 })
  expect(pronto.trajeto.at(-1)).toMatchObject({ x: 3, y: 3 })

  const rotulo = await rotuloDaExecucao(id)
  await page.goto(`/execucoes/${id}`)
  await expect(page.getByRole('heading', { name: `Execução ${rotulo}` })).toBeVisible()
  const linha = page.getByRole('row').nth(1)
  await expect(linha).toContainText('1/3')
  await expect(linha).toContainText('Sucesso')
  await expect(linha).toContainText(formatarTempo(pronto.tentativas[0].tempo_s))
  await expect(linha).toContainText(formatarMedicao(pronto.tentativas[0].velocidade_media, 'm/s'))
  await expect(page.getByText('OBJETIVO', { exact: true })).toBeVisible()
  expect(await page.evaluate(caminhoLivre)).toBe(true)
})

test('E2E-02a: sem health-check válido a Nova execução não abre (DIP divergente e reprovado)', async ({ page }) => {
  await page.goto('/')
  const solicitar = page.getByRole('button', { name: 'Solicitar health-check', exact: true })
  const iniciar = page.getByRole('button', { name: /Nova execução/ })

  const divergente = await hcVariado('dip', (linha) => { if (linha.tipo === 'hc_resultado') linha.tipo_dip = '8x4' })
  await expect(solicitar).toBeEnabled()
  await solicitar.click()
  await reproduzir(divergente, bootNovo())
  await expect(page.getByText('O robô informou 8x4; o labirinto selecionado é 4x4.')).toBeVisible()
  await expect(iniciar).toBeDisabled()
  expect((await fetch(`${API}/execucoes/em-andamento`)).status).toBe(204)
  const recusaDip = await abrirExecucaoPelaApi()
  expect(recusaDip.status).toBe(409)
  expect(await recusaDip.json()).toMatchObject({ motivo: 'dip_divergente' })

  const reprovado = await hcVariado('reprovado', (linha) => {
    if (linha.tipo === 'hc_resultado') linha.aprovado = false
    if (linha.tipo === 'hc_item' && linha.componente === 'tof_frontal') linha.aprovado = false
  })
  await expect(solicitar).toBeEnabled()
  await solicitar.click()
  await reproduzir(reprovado, bootNovo())
  await expect(page.getByText('Health-check reprovado. Corrija a falha e solicite outra verificação.')).toBeVisible()
  await expect(iniciar).toBeDisabled()
  expect((await fetch(`${API}/execucoes/em-andamento`)).status).toBe(204)
  const recusaReprovado = await abrirExecucaoPelaApi()
  expect(recusaReprovado.status).toBe(409)
  expect(await recusaReprovado.json()).toMatchObject({ motivo: 'health_check_reprovado' })
  expect((await fetch(`${API}/execucoes/em-andamento`)).status).toBe(204)
})

test('E2E-02b: Nova execução com uma execução em andamento cancela a anterior', async ({ page }) => {
  await prepararLargada(page, bootNovo())
  const anterior = await execucaoAtual()
  expect(anterior.tentativas[0].status).toBe('running')

  await page.getByRole('button', { name: /\+ Nova execução/ }).click()
  const modal = page.getByRole('dialog')
  await expect(modal).toContainText('será cancelada')
  await modal.getByRole('button', { name: 'Confirmar nova execução' }).click()
  await expect(modal).toHaveCount(0)

  await expect.poll(async () => (await execucaoAtual()).execucao_id).not.toBe(anterior.execucao_id)
  const nova = await execucaoAtual()
  expect(nova.status).toBe('em_andamento')
  expect(nova.tentativas_usadas).toBe(1)
  expect(nova.tentativas[0]).toMatchObject({ attempt_index: 1, status: 'running' })
  await expect(page.getByLabel('Tentativa 1 de 3')).toBeVisible()

  const velha = await detalhe(anterior.execucao_id)
  expect(velha.status).toBe('cancelada')
  expect(velha.tentativas[0].status).toBe('failed')

  await page.goto('/execucoes')
  await expect(page.getByRole('link', { name: await rotuloDaExecucao(nova.execucao_id) })).toBeVisible()
  await expect(page.getByRole('link', { name: await rotuloDaExecucao(anterior.execucao_id) })).toBeVisible()
})

test('E2E-06: 10 s sem telemetria encerram a tentativa por link_lost e permitem a retomada', async ({ page }) => {
  const boot = bootNovo()
  await prepararLargada(page, boot)
  await reproduzir(gravacao.trecho, boot)
  const id = (await execucaoAtual()).execucao_id
  await expect(page.getByLabel('Dados da execução em tempo real').getByText('A2', { exact: true })).toBeVisible()

  await expect.poll(async () => (await detalhe(id)).tentativas[0].status, { timeout: 30_000, intervals: [1_000] }).toBe('failed')
  const depois = await detalhe(id)
  expect(depois.status).toBe('em_andamento')
  expect(depois.tentativas[0].falha).toMatchObject({
    motivo: 'link_lost', origem: 'automatica', celula_x: 0, celula_y: 1, componente: null,
  })
  await expect(page.getByText(/Tentativa 1 registrada como Falha/)).toBeVisible({ timeout: 15_000 })
  await expect(page.getByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeVisible()
  await expect(page.getByRole('button', { name: 'Encerrar tentativa', exact: true })).toHaveCount(0)
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

async function hcVariado(nome: string, alterar: (linha: Record<string, unknown>) => void) {
  const linhas = (await readFile(gravacao.hc, 'utf8')).trim().split('\n').map((linha) => JSON.parse(linha) as Record<string, unknown>)
  linhas.forEach(alterar)
  const arquivo = path.join(pasta, `hc-${nome}.jsonl`)
  await writeFile(arquivo, `${linhas.map((linha) => JSON.stringify(linha)).join('\n')}\n`)
  return arquivo
}

function abrirExecucaoPelaApi() {
  return fetch(`${API}/execucoes`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ tipo_labirinto: '4x4' }),
  })
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

interface ResumoApi {
  execucao_id: string
  numero?: number
}

interface DetalheApi {
  execucao_id: string
  numero?: number
  status: string
  tipo_labirinto: string
  variacao_melhor_tempo_pct: number | null
  tentativas: {
    attempt_index: number
    status: string
    tempo_s: number | null
    velocidade_media: number | null
    consumo_bateria: number | null
    falha: FalhaApi | null
  }[]
  trajeto: { x: number; y: number; retomada: boolean; paredes_mask: number }[]
}

async function detalhe(id: string) {
  const resposta = await fetch(`${API}/execucoes/${id}`)
  if (!resposta.ok) throw new Error(`GET /execucoes/${id} respondeu ${resposta.status}.`)
  const corpo = await resposta.json() as DetalheApi
  corpo.tentativas.sort((a, b) => a.attempt_index - b.attempt_index)
  return corpo
}

async function rotuloDaExecucao(id: string) {
  const corpo = await detalhe(id)
  return corpo.numero === undefined ? corpo.execucao_id : `#${String(corpo.numero).padStart(4, '0')}`
}

async function listar(tipo: string | null) {
  const dados: ResumoApi[] = []
  for (let offset = 0; ; offset += 100) {
    const filtro = tipo ? `tipo_labirinto=${tipo}&` : ''
    const resposta = await fetch(`${API}/execucoes?${filtro}limite=100&offset=${offset}`)
    if (!resposta.ok) throw new Error(`GET /execucoes respondeu ${resposta.status}.`)
    const pagina = await resposta.json() as ResumoApi[]
    dados.push(...pagina)
    if (pagina.length < 100) break
  }
  return dados.map((item) => item.numero === undefined ? item.execucao_id : `#${String(item.numero).padStart(4, '0')}`)
}

async function conferirLista(page: Page, tipo: string | null) {
  await expect.poll(async () => {
    const esperada = await listar(tipo)
    const visivel = await page.getByRole('table').getByRole('link').allTextContents()
    return visivel.join('\n') === esperada.join('\n')
  }).toBe(true)
}

function nomeCelula(x: number, y: number) {
  return `${String.fromCharCode(65 + x)}${y + 1}`
}

function letra(celula: string) {
  return celula.charCodeAt(0) - 65
}

function numeroCelula(celula: string) {
  return Number(celula.slice(1)) - 1
}

function formatarTempo(segundos: number | null) {
  if (segundos == null) return '—'
  const decimos = Math.round(segundos * 10)
  const minutos = Math.floor(decimos / 600)
  const resto = (decimos % 600) / 10
  const texto = resto.toLocaleString('pt-BR', { minimumFractionDigits: 1, maximumFractionDigits: 1 }).padStart(4, '0')
  return `${String(minutos).padStart(2, '0')}:${texto}`
}

function formatarMedicao(valor: number | null, unidade: string) {
  return valor == null ? '—' : `${String(valor).replace('.', ',')} ${unidade}`
}

/** Divide a corrida gravada em três trechos, parando nas células em que o operador interrompe. */
function fatiarCorrida(texto: string) {
  const msgs = texto.trim().split('\n').filter(Boolean).map((linha) => ({
    linha,
    json: JSON.parse(linha) as { tipo: string; x?: number; y?: number },
  }))
  const inicio = msgs.findIndex((msg) => msg.json.tipo === 'hc_resultado')
  const corrida = msgs.slice(inicio + 1)
  const passos = corrida.map((msg, indice) => msg.json.tipo === 'passo' ? indice : -1).filter((indice) => indice >= 0)
  if (passos.length < 3) throw new Error(`A gravação tem ${passos.length} passos; as três tentativas precisam de ao menos um cada.`)
  const limites = [passos[Math.floor(passos.length / 3)], passos[Math.floor((2 * passos.length) / 3)], corrida.length]
  let de = 0
  const trechos = limites.map((ate) => {
    const fatia = `${corrida.slice(de, ate).map((msg) => msg.linha).join('\n')}\n`
    de = ate
    return fatia
  })
  const paradas = trechos.slice(0, 2).map((fatia) => {
    const itens = fatia.trim().split('\n').map((linha) => JSON.parse(linha) as { tipo: string; x?: number; y?: number })
    const tel = [...itens].reverse().find((msg) => msg.tipo === 'tel' && msg.x != null && msg.y != null)
    if (!tel || tel.x == null || tel.y == null) throw new Error('O trecho não tem a célula em que o operador interrompe.')
    return nomeCelula(tel.x, tel.y)
  })
  const sucesso = [...msgs].reverse().find((msg) => msg.json.tipo === 'sucesso')
  if (!sucesso || sucesso.json.x == null || sucesso.json.y == null) throw new Error('A gravação não termina em sucesso.')
  return { trechos, paradas, objetivo: nomeCelula(sucesso.json.x, sucesso.json.y) }
}

/** Roda no navegador: true quando nenhum segmento do trajeto coincide com uma parede. */
function caminhoLivre() {
  const pontos = Array.from(document.querySelectorAll('polyline.labirinto__trajeto'))
    .flatMap((linha) => (linha.getAttribute('points') ?? '').trim().split(/\s+/).filter(Boolean))
    .map((par) => par.split(',').map(Number))
  const paredes = Array.from(document.querySelectorAll('.labirinto__parede line')).map((linha) => ({
    x1: Number(linha.getAttribute('x1')),
    y1: Number(linha.getAttribute('y1')),
    x2: Number(linha.getAttribute('x2')),
    y2: Number(linha.getAttribute('y2')),
  }))
  if (paredes.length === 0) return false
  return pontos.slice(1).every(([x2, y2], i) => {
    const [x1, y1] = pontos[i]
    if (x1 === x2 && y1 === y2) return true
    const mx = (x1 + x2) / 2
    const my = (y1 + y2) / 2
    return !paredes.some((parede) => {
      if (y1 !== y2 && parede.y1 === parede.y2 && parede.y1 === my) {
        return mx > Math.min(parede.x1, parede.x2) && mx < Math.max(parede.x1, parede.x2)
      }
      if (x1 !== x2 && parede.x1 === parede.x2 && parede.x1 === mx) {
        return my > Math.min(parede.y1, parede.y2) && my < Math.max(parede.y1, parede.y2)
      }
      return false
    })
  })
}
