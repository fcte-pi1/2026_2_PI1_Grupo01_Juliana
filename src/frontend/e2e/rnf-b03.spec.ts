import { expect, test } from '@playwright/test'

// Caso 4.9 do 7.4 (RNF-B03): em 1366x768 a tela da execução mostra, sem rolagem,
// status, tempo decorrido, célula atual, nível de bateria e velocidade média.
test.use({ viewport: { width: 1366, height: 768 } })

test('1366x768: os cinco dados da execução aparecem sem rolagem', async ({ page }, testInfo) => {
  await page.goto('/?cenario=03-em-execucao')
  await expect(page.getByLabel('Tentativa 1 de 3')).toBeVisible()

  const campos = {
    status: page.locator('.pilula', { hasText: 'Em execução' }).first(),
    'tempo decorrido': page.locator('.rotulo', { hasText: 'Tempo da tentativa' }),
    'célula atual': page.locator('.rotulo', { hasText: 'Célula atual' }),
    'nível de bateria': page.locator('.rotulo', { hasText: /^Bateria$/ }),
    'velocidade média': page.getByText(/Velocidade média:/),
  }
  for (const campo of Object.values(campos)) {
    await expect(campo).toBeVisible()
    await expect(campo).toBeInViewport({ ratio: 1 })
  }
  expect(await page.evaluate(() => window.scrollY)).toBe(0)
  await page.screenshot({ path: testInfo.outputPath('rnf-b03-1366x768.png') })
})
