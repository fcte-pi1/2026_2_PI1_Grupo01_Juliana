/*
 * Testes de energia (7.2): lê os CSV de docs/dados/energia/ e monta os gráficos
 * e as contas da página. Quem executa o teste só preenche o CSV.
 *
 * CSV com ";" entre colunas, vírgula ou ponto nos decimais e "#" para comentário.
 */

const numero = (texto) => {
  if (texto === undefined || texto.trim() === "") return null
  const valor = Number(texto.trim().replace(",", "."))
  return Number.isFinite(valor) ? valor : null
}

const formatar = (valor, casas) =>
  valor === null || !Number.isFinite(valor) ? "—" : valor.toLocaleString("pt-BR", {
    minimumFractionDigits: casas, maximumFractionDigits: casas,
  })

async function lerCsv(caminho) {
  const resposta = await fetch(new URL(caminho, location.href), { cache: "no-store" })
  if (!resposta.ok) throw new Error(`não foi possível ler ${caminho}`)
  const linhas = (await resposta.text()).split(/\r?\n/)
    .filter((l) => l.trim() !== "" && !l.trim().startsWith("#"))
  const [cabecalho, ...dados] = linhas
  const colunas = cabecalho.split(";").map((c) => c.trim())
  return dados.map((linha) => {
    const valores = linha.split(";")
    return Object.fromEntries(colunas.map((c, i) => [c, (valores[i] ?? "").trim()]))
  })
}

function tabela(cabecalho, linhas) {
  const t = document.createElement("table")
  t.innerHTML = `<thead><tr>${cabecalho.map((c) => `<th>${c}</th>`).join("")}</tr></thead>` +
    `<tbody>${linhas.map((l) => `<tr>${l.map((c) => `<td>${c}</td>`).join("")}</tr>`).join("")}</tbody>`
  return t
}

function aviso(alvo, texto) {
  const p = document.createElement("p")
  p.className = "energia-aviso"
  p.textContent = texto
  alvo.replaceChildren(p)
}

function cor(variavel, reserva) {
  return getComputedStyle(document.body).getPropertyValue(variavel).trim() || reserva
}

/* Linhas horizontais tracejadas com rótulo (alerta e corte), sem virar série do gráfico. */
const linhasDeReferencia = {
  id: "linhasDeReferencia",
  afterDatasetsDraw(chart, _args, opcoes) {
    const { ctx, chartArea, scales } = chart
    for (const ref of opcoes.linhas ?? []) {
      const y = scales.y.getPixelForValue(ref.valor)
      if (y < chartArea.top || y > chartArea.bottom) continue
      ctx.save()
      ctx.strokeStyle = opcoes.cor
      ctx.fillStyle = opcoes.cor
      ctx.setLineDash([4, 4])
      ctx.lineWidth = 1
      ctx.beginPath()
      ctx.moveTo(chartArea.left, y)
      ctx.lineTo(chartArea.right, y)
      ctx.stroke()
      ctx.setLineDash([])
      ctx.font = "12px sans-serif"
      ctx.textAlign = "right"
      ctx.fillText(ref.rotulo, chartArea.right - 4, y - 4)
      ctx.restore()
    }
  },
}

function grafico(alvo, rotulo, pontos, unidade, casas, referencias = []) {
  const figura = document.createElement("div")
  figura.className = "energia-grafico"
  const canvas = document.createElement("canvas")
  canvas.setAttribute("role", "img")
  canvas.setAttribute("aria-label", `${rotulo} pelo tempo`)
  figura.append(canvas)
  alvo.append(figura)

  const tinta = cor("--md-default-fg-color--light", "#666")
  const grade = cor("--md-default-fg-color--lightest", "#ddd")
  const serie = cor("--md-primary-fg-color", "#2a6fdb")
  return new Chart(canvas, {
    type: "line",
    data: {
      datasets: [{
        label: rotulo, data: pontos, borderColor: serie, backgroundColor: serie,
        borderWidth: 2, pointRadius: 0, pointHoverRadius: 5, pointHitRadius: 12, tension: 0,
      }],
    },
    options: {
      maintainAspectRatio: false,
      animation: false,
      interaction: { mode: "index", intersect: false },
      plugins: {
        legend: { display: false },
        title: { display: true, text: `${rotulo} (${unidade})`, color: tinta, align: "start" },
        tooltip: {
          callbacks: {
            title: (itens) => `${formatar(itens[0].parsed.x, 0)} min`,
            label: (item) => `${rotulo}: ${formatar(item.parsed.y, casas)} ${unidade}`,
          },
        },
        linhasDeReferencia: { linhas: referencias, cor: tinta },
      },
      scales: {
        x: {
          type: "linear", title: { display: true, text: "Tempo (min)", color: tinta },
          ticks: { color: tinta }, grid: { color: grade },
        },
        /* O eixo desce até a menor linha de referência, para o alerta e o corte aparecerem. */
        y: {
          suggestedMin: referencias.length ? Math.min(...referencias.map((r) => r.valor)) - 0.1 : undefined,
          ticks: { color: tinta }, grid: { color: grade },
        },
      },
    },
    plugins: [linhasDeReferencia],
  })
}

async function montarDescarga(alvo) {
  const d = alvo.dataset
  const alerta = numero(d.alerta)
  const corte = numero(d.corte)
  const capacidade = numero(d.capacidade)
  const correntePrevista = numero(d.correntePrevista)
  const minimo = numero(d.minimoMin)
  const tensaoMotores = numero(d.tensaoMotores)

  /*
   * Com a fonte de bancada, os motores ficam num canal à parte (corrente_motores_A).
   * A corrente que sairia da bateria soma o canal do circuito com a dos motores
   * convertida pela potência: Im x Vmotores / Vbateria, como faria o PWM da ponte H.
   */
  const linhas = (await lerCsv(d.csv))
    .map((l) => {
      const v = numero(l.v_bateria_V)
      const circuito = numero(l.corrente_A)
      const motores = numero(l.corrente_motores_A)
      const i = motores !== null && tensaoMotores !== null && v
        ? (circuito ?? 0) + motores * tensaoMotores / v
        : circuito
      return { ...l, t: numero(l.tempo_min), v, i, motores }
    })
    .filter((l) => l.t !== null && l.v !== null)
    .sort((a, b) => a.t - b.t)
  const emulada = linhas.some((l) => l.motores !== null)
  if (linhas.length < 2) {
    aviso(alvo, "Aguardando os dados do teste: preencha o arquivo " + d.csv.replace(/^(\.\.\/)+/, "docs/") + ".")
    return
  }
  alvo.replaceChildren()
  if (emulada) {
    aviso(alvo, `Descarga emulada com fonte de bancada: a tensão segue a curva típica da LiPo, e a corrente da bateria é a do circuito mais a dos motores × ${formatar(tensaoMotores, 1)} V ÷ tensão da bateria.`)
  }

  grafico(alvo, "Tensão da bateria", linhas.map((l) => ({ x: l.t, y: l.v })), "V", 2, [
    ...(alerta !== null ? [{ valor: alerta, rotulo: `alerta ${formatar(alerta, 1)} V` }] : []),
    ...(corte !== null ? [{ valor: corte, rotulo: `corte ${formatar(corte, 1)} V` }] : []),
  ])
  const comCorrente = linhas.filter((l) => l.i !== null)
  if (comCorrente.length >= 2) {
    grafico(alvo, "Corrente total", comCorrente.map((l) => ({ x: l.t, y: l.i })), "A", 3)
  }

  /* Energia e carga pela regra dos trapézios entre leituras seguidas. */
  let energiaJ = 0
  let cargaMah = 0
  for (let k = 1; k < comCorrente.length; k++) {
    const a = comCorrente[k - 1]
    const b = comCorrente[k]
    const dtS = (b.t - a.t) * 60
    energiaJ += ((a.v * a.i + b.v * b.i) / 2) * dtS
    cargaMah += ((a.i + b.i) / 2) * (dtS / 3600) * 1000
  }
  const primeira = linhas[0]
  const ultima = linhas.at(-1)
  const duracao = ultima.t - primeira.t
  const correnteMedia = comCorrente.length ? comCorrente.reduce((s, l) => s + l.i, 0) / comCorrente.length : null
  const chegouNoCorte = corte !== null ? linhas.find((l) => l.v <= corte) : undefined
  const autonomia = capacidade !== null && correnteMedia ? (0.8 * capacidade) / (correnteMedia * 1000) * 60 : null

  const media = (valores) => valores.length ? valores.reduce((s, x) => s + x, 0) / valores.length : null
  const pares = linhas.map((l) => ({ ...l, diodo: numero(l.v_depois_diodo_V), divisor: numero(l.v_divisor_V) }))
  const quedaDiodo = media(pares.filter((l) => l.diodo !== null).map((l) => l.v - l.diodo))
  const razaoDivisor = media(pares.filter((l) => l.diodo !== null && l.divisor).map((l) => l.diodo / l.divisor))
  const faixa = (coluna) => {
    const valores = linhas.map((l) => numero(l[coluna])).filter((x) => x !== null)
    return valores.length ? `${formatar(Math.min(...valores), 2)} a ${formatar(Math.max(...valores), 2)} V` : "—"
  }
  const atende = (ok) => ok === null ? "—" : ok ? "Sim" : "Não"

  const resumo = [
    ["Duração do teste", `${formatar(duracao, 0)} min`, minimo !== null ? `≥ ${formatar(minimo, 0)} min` : "—", atende(minimo !== null ? duracao >= minimo : null)],
    ["Tensão no início e no fim", `${formatar(primeira.v, 2)} → ${formatar(ultima.v, 2)} V`, "—", "—"],
    ["Taxa média de queda", `${formatar((ultima.v - primeira.v) / duracao, 3)} V/min`, "—", "—"],
    ["Menor tensão", `${formatar(Math.min(...linhas.map((l) => l.v)), 2)} V`, corte !== null ? `acima do corte de ${formatar(corte, 1)} V` : "—",
      atende(corte !== null ? !chegouNoCorte : null)],
    ["Corrente média", `${formatar(correnteMedia, 3)} A`, correntePrevista !== null ? `≤ ${formatar(correntePrevista * 1.3, 2)} A (${formatar(correntePrevista, 2)} A + 30%)` : "—",
      atende(correnteMedia !== null && correntePrevista !== null ? correnteMedia <= correntePrevista * 1.3 : null)],
    ["Carga usada", capacidade !== null ? `${formatar(cargaMah, 0)} mAh (${formatar(cargaMah / capacidade * 100, 0)}% de ${formatar(capacidade, 0)} mAh)` : `${formatar(cargaMah, 0)} mAh`, "—", "—"],
    ["Energia consumida", `${formatar(energiaJ, 0)} J (${formatar(energiaJ / 3600, 3)} Wh)`, "—", "—"],
    ["Autonomia estimada (80% da carga)", `${formatar(autonomia, 0)} min`, "≥ 30 min (3 tentativas de 10 min)", atende(autonomia !== null ? autonomia >= 30 : null)],
    ["Queda média no diodo SS34", quedaDiodo !== null ? `${formatar(quedaDiodo, 2)} V` : "—", "0,30 a 0,40 V", atende(quedaDiodo !== null ? quedaDiodo >= 0.3 && quedaDiodo <= 0.4 : null)],
    ["Razão do divisor do ADC", formatar(razaoDivisor, 2), "3,20 ± 2%", atende(razaoDivisor !== null ? Math.abs(razaoDivisor - 3.2) <= 0.064 : null)],
    ["Linha de 5 V", faixa("v_5V_V"), "4,75 a 5,25 V", "ver a faixa"],
    ["Linha de 3,3 V", faixa("v_3v3_V"), "3,14 a 3,47 V", "ver a faixa"],
  ]
  const titulo = document.createElement("p")
  titulo.innerHTML = "<strong>Resultados calculados a partir do CSV</strong>"
  alvo.append(titulo, tabela(["Grandeza", "Medido", "Esperado", "Atende"], resumo))

  const detalhes = document.createElement("details")
  detalhes.innerHTML = "<summary>Leituras registradas</summary>"
  detalhes.append(tabela(["Tempo (min)", "Bateria (V)", "Corrente total (A)", "Motores (A)", "Depois do diodo (V)", "Divisor (V)", "5 V", "3,3 V", "Observação"],
    linhas.map((l) => [formatar(l.t, 0), formatar(l.v, 2), formatar(l.i, 3), formatar(l.motores, 3), formatar(numero(l.v_depois_diodo_V), 2),
      formatar(numero(l.v_divisor_V), 2), formatar(numero(l.v_5V_V), 2), formatar(numero(l.v_3v3_V), 2), l.observacao ?? ""])))
  alvo.append(detalhes)
}

async function montarConsumo(alvo) {
  const linhas = await lerCsv(alvo.dataset.csv)
  const medidas = linhas.filter((l) => numero(l.corrente_medida_mA) !== null)
  const corpo = linhas.map((l) => {
    const prevista = numero(l.corrente_prevista_mA)
    const medida = numero(l.corrente_medida_mA)
    const tensao = numero(l.tensao_medida_V) ?? numero(l.tensao_prevista_V)
    const diferenca = prevista && medida !== null ? (medida - prevista) / prevista * 100 : null
    return [l.componente, formatar(tensao, 2), formatar(prevista, 0), formatar(medida, 0),
      formatar(medida !== null && tensao !== null ? tensao * medida / 1000 : null, 3),
      diferenca === null ? "—" : `${diferenca > 0 ? "+" : ""}${formatar(diferenca, 0)}%`,
      diferenca === null ? "—" : diferenca <= 30 ? "Sim" : "Não", l.observacao ?? ""]
  })
  alvo.replaceChildren(tabela(["Componente", "Tensão (V)", "Prevista (mA)", "Medida (mA)", "Potência medida (W)",
    "Diferença", "Até +30%", "Observação"], corpo))
  if (!medidas.length) {
    const p = document.createElement("p")
    p.className = "energia-aviso"
    p.textContent = "Aguardando as medições: preencha a coluna corrente_medida_mA de docs/dados/energia/consumo-componentes.csv."
    alvo.append(p)
  }
}

document$.subscribe(() => {
  document.querySelectorAll(".energia-descarga").forEach((alvo) =>
    montarDescarga(alvo).catch((erro) => aviso(alvo, `Erro ao montar o gráfico: ${erro.message}`)))
  document.querySelectorAll(".energia-consumo").forEach((alvo) =>
    montarConsumo(alvo).catch((erro) => aviso(alvo, `Erro ao montar a tabela: ${erro.message}`)))
})
