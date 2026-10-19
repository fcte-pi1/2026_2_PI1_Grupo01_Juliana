import { expect, test, type Page } from '@playwright/test'

async function abrirCenario(page: Page, cenario: string) {
  await page.goto(`/?cenario=${cenario}`)
  await expect(page.getByRole('heading', { name: 'Execução ativa #0042' })).toBeVisible()
}

async function buscarExecucao(page: Page) {
  return page.evaluate(async () => {
    const resposta = await fetch('/api/execucoes/e-0042')
    return resposta.json()
  })
}

test.describe('Encerrar execução', () => {
  test('permite cancelar o modal sem alterar a execução', async ({ page }) => {
    const pedidos: string[] = []
    page.on('request', (pedido) => {
      if (pedido.method() === 'POST' && pedido.url().endsWith('/execucoes/e-0042/encerrar')) {
        pedidos.push(pedido.url())
      }
    })
    await abrirCenario(page, '02-health-check')

    await page.getByRole('button', { name: 'Encerrar execução' }).click()
    const modal = page.getByRole('dialog', { name: 'Encerrar a execução #0042 por completo?' })
    await expect(modal).toContainText('Não será possível retomá-la')
    await expect(modal).toContainText('pare-o pelo botão BOOT')

    await modal.getByRole('button', { name: 'Cancelar' }).click()

    await expect(modal).toBeHidden()
    await expect(page.getByRole('heading', { name: 'Execução ativa #0042' })).toBeVisible()
    expect(pedidos).toHaveLength(0)
    expect((await buscarExecucao(page)).status).toBe('em_andamento')
  })

  test('encerra a tentativa aberta e cancela a execução sem permitir retomada', async ({ page }) => {
    const pedidos: string[] = []
    page.on('request', (pedido) => {
      if (pedido.method() === 'POST' && pedido.url().endsWith('/execucoes/e-0042/encerrar')) {
        pedidos.push(pedido.url())
      }
    })
    await abrirCenario(page, '02-health-check')

    await page.getByRole('button', { name: 'Encerrar execução' }).click()
    const modal = page.getByRole('dialog', { name: 'Encerrar a execução #0042 por completo?' })
    await modal.getByRole('button', { name: 'Encerrar execução' }).click()

    await expect(page.getByRole('heading', { name: 'Execução cancelada #0042' })).toBeVisible()
    await expect(page.getByText(/esta execução não aceita novas tentativas/)).toBeVisible()
    await expect(page.getByRole('button', { name: /Retomar/ })).toHaveCount(0)
    await expect(page.getByRole('button', { name: 'Encerrar execução' })).toHaveCount(0)

    expect(pedidos).toHaveLength(1)
    const execucao = await buscarExecucao(page)
    expect(execucao.status).toBe('cancelada')
    expect(execucao.encerrada_em).not.toBeNull()
    expect(execucao.tentativas[0]).toMatchObject({
      status: 'failed',
      falha: {
        motivo: 'encerrado_operador',
        origem: 'encerrado_operador',
      },
    })
  })

  test('cancela a execução após uma tentativa falhar e remove a retomada', async ({ page }) => {
    await abrirCenario(page, '08-encerrada-como-falha')
    await expect(page.getByRole('button', { name: /Retomar tentativa/ })).toBeVisible()

    await page.getByRole('button', { name: 'Encerrar execução' }).click()
    const modal = page.getByRole('dialog', { name: 'Encerrar a execução #0042 por completo?' })
    await expect(modal).toContainText('não aceitará novas tentativas')
    await modal.getByRole('button', { name: 'Encerrar execução' }).click()

    await expect(page.getByRole('heading', { name: 'Execução cancelada #0042' })).toBeVisible()
    await expect(page.getByRole('button', { name: /Retomar/ })).toHaveCount(0)

    const execucao = await buscarExecucao(page)
    expect(execucao.status).toBe('cancelada')
    expect(execucao.tentativas).toHaveLength(1)
    expect(execucao.tentativas[0].status).toBe('failed')
  })
})
