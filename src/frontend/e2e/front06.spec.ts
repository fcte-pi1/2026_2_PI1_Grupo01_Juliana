import { expect, test, type Page } from '@playwright/test'
import { detalheAPI } from '../src/mocks/dados'
import { execucaoUs28 } from '../src/testes/execucaoUs28'

async function titulos(page: Page) {
  return page.locator('svg.labirinto__svg title').allTextContents()
}

test('histórico desce no tempo e o filtro separa os labirintos', async ({ page }) => {
  await page.goto('/execucoes')
  const links = page.getByRole('table').getByRole('link')
  await expect(links).toHaveText(['#0041', '#0040', '#0036', '#0034', '#0032'])

  const filtro = page.getByRole('group', { name: 'Filtrar por labirinto' })
  await filtro.getByRole('button', { name: '4x4' }).click()
  await expect(links).toHaveText(['#0041', '#0036'])
  await filtro.getByRole('button', { name: '8x4' }).click()
  await expect(links).toHaveText(['#0040', '#0032'])
  await filtro.getByRole('button', { name: '12x4' }).click()
  await expect(links).toHaveText(['#0034'])
  await filtro.getByRole('button', { name: 'Todos' }).click()
  await expect(links).toHaveCount(5)
})

test('detalhe reexibe as três tentativas e o trajeto que não atravessa parede', async ({ page }) => {
  const corpo = JSON.stringify(detalheAPI(execucaoUs28()))
  await page.addInitScript((resposta) => {
    const buscar = window.fetch.bind(window)
    window.fetch = (entrada, opcoes) => {
      const url = typeof entrada === 'string' ? entrada : entrada instanceof URL ? entrada.href : entrada.url
      if (new URL(url, window.location.origin).pathname === '/api/execucoes/e-0043') {
        return Promise.resolve(new Response(resposta, { status: 200, headers: { 'Content-Type': 'application/json' } }))
      }
      return buscar(entrada, opcoes)
    }
  }, corpo)
  await page.goto('/execucoes/e-0043')
  await expect(page.getByRole('heading', { name: 'Execução #0043' })).toBeVisible()
  await expect(page.getByText('12,5')).toBeVisible()
  await expect(page.getByRole('row').nth(1)).toContainText('1/3')
  await expect(page.getByRole('row').nth(1)).toContainText('00:06,2')
  await expect(page.getByRole('row').nth(2)).toContainText('00:04,8')
  await expect(page.getByRole('row').nth(3)).toContainText('Sucesso')
  await expect(page.getByRole('row').nth(3)).toContainText('00:09,5')

  const falha1 = page.locator('section').filter({ has: page.getByRole('heading', { name: 'Registro da falha · tentativa 1' }) })
  await expect(falha1).toContainText('A3')
  await expect(falha1).toContainText('Colisão com parede')
  await expect(falha1).toContainText('Encerrada pelo operador')
  const falha2 = page.locator('section').filter({ has: page.getByRole('heading', { name: 'Registro da falha · tentativa 2' }) })
  await expect(falha2).toContainText('B4')
  await expect(falha2).toContainText('Robô travado ou parado')
  await expect(page.getByRole('heading', { name: 'Registro da falha · tentativa 3' })).toHaveCount(0)

  await expect.poll(() => titulos(page)).toEqual([
    'A1 · passo 1', 'A2 · passo 2', 'A3 · passo 4', 'A4 · passo 5',
    'B4 · passo 7', 'C4 · passo 8', 'D4 · passo 9', 'Robô em D4',
  ])
  await expect(page.locator('.labirinto__robo title')).toHaveText('Robô em D4')
  expect(await page.evaluate(caminhoLivre)).toBe(true)

  const grupo = page.getByRole('group', { name: 'Trajeto por tentativa' })
  await grupo.getByRole('button', { name: 'T1' }).click()
  await expect(page.locator('.labirinto__robo title')).toHaveText('Robô em A3 · falhou')
  await expect(page.getByText('FALHA T2')).toBeVisible()
  await grupo.getByRole('button', { name: 'T2' }).click()
  await expect(page.getByText('Retomada em A3')).toBeVisible()
  await expect(page.locator('.labirinto__robo title')).toHaveText('Robô em B4 · falhou')
  await grupo.getByRole('button', { name: 'T3' }).click()
  await expect(page.getByText('Retomada em B4')).toBeVisible()
  await expect(page.locator('.labirinto__robo title')).toHaveText('Robô em D4')
  await expect.poll(() => titulos(page)).toEqual(expect.arrayContaining(['B4 · passo 7', 'C4 · passo 8', 'D4 · passo 9']))
  expect(await page.evaluate(caminhoLivre)).toBe(true)
})

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
