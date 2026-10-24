import { expect, test } from '@playwright/test'

// Demonstração com MSW. A suíte us-conectado.spec.ts valida o transporte real.
test('FRONT-01/06 — US03, US26, US27, US28: navega do início ao detalhe e retorna', async ({ page }, testInfo) => {
  await page.goto('/')
  const menu = page.getByRole('navigation', { name: 'Principal' })
  await expect(menu.getByRole('link', { name: 'Início', exact: true })).toHaveAttribute('aria-current', 'page')
  await menu.getByRole('link', { name: 'Histórico de execuções' }).click()
  await expect(page.getByRole('table').getByRole('link')).toHaveCount(5)
  await expect(menu.getByRole('link', { name: 'Histórico de execuções' })).toHaveAttribute('aria-current', 'page')
  await page.getByRole('group', { name: 'Filtrar por labirinto' }).getByRole('button', { name: '4x4', exact: true }).click()
  await expect(page).toHaveURL(/\/execucoes\?labirinto=4x4$/)
  await expect(page.getByRole('table').getByRole('link')).toHaveText(['#0041', '#0036'])
  await page.getByRole('link', { name: '#0041', exact: true }).click()
  await expect(page.getByRole('heading', { name: 'Execução #0041' })).toBeVisible()
  await expect(page.getByRole('heading', { name: 'Tentativas em ordem cronológica' })).toBeVisible()
  await expect(page.getByRole('heading', { name: 'Health-check' })).toBeVisible()
  await page.screenshot({ path: testInfo.outputPath('us28-detalhe.png'), fullPage: true })
  await page.getByRole('link', { name: 'Voltar ao histórico' }).click()
  await expect(page.getByRole('table').getByRole('link')).toHaveCount(5)
  await menu.getByRole('link', { name: 'Início', exact: true }).click()
  await expect(page).toHaveURL(/\/$/)
  await expect(page.getByRole('button', { name: /Nova execução/ })).toBeVisible()
})

for (const [tipo, registros] of [
  ['4x4', ['#0041', '#0036']],
  ['8x4', ['#0040', '#0032']],
  ['12x4', ['#0034']],
] as const) {
  test(`FRONT-01/06 — US26: mantém o filtro ${tipo} após recarregar a rota`, async ({ page }) => {
    await page.goto(`/execucoes?labirinto=${tipo}`)
    const filtro = page.getByRole('group', { name: 'Filtrar por labirinto' })
    await expect(filtro.getByRole('button', { name: tipo, exact: true })).toHaveAttribute('aria-pressed', 'true')
    const tabela = page.getByRole('table')
    await expect(tabela.getByRole('link')).toHaveText([...registros])
    await expect(tabela.getByRole('cell', { name: tipo, exact: true })).toHaveCount(registros.length)
    await page.reload()
    await expect(tabela.getByRole('link')).toHaveText([...registros])
    await expect(tabela.getByRole('cell', { name: tipo, exact: true })).toHaveCount(registros.length)
    await expect(filtro.getByRole('button', { name: tipo, exact: true })).toHaveAttribute('aria-pressed', 'true')
    await filtro.getByRole('button', { name: 'Todos', exact: true }).click()
    await expect(page).toHaveURL(/\/execucoes$/)
    await expect(tabela.getByRole('link')).toHaveCount(5)
  })
}
