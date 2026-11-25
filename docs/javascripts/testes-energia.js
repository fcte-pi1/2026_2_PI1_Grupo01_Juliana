/*
 * Testes de energia (7.2): lê os CSV de docs/dados/energia/ e monta os gráficos
 * e as contas da página. Quem executa o teste só preenche o CSV.
 *
 * CSV com ";" entre colunas, vírgula ou ponto nos decimais e "#" para comentário.
 * A exportação das execuções (teste 5.44 do 7.5) vem do back-end com "," entre as colunas
 * e é lida como está, sem editar.
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

async function lerCsv(caminho, separador = ";") {
  const resposta = await fetch(new URL(caminho, location.href), { cache: "no-store" })
  if (!resposta.ok) throw new Error(`não foi possível ler ${caminho}`)
  const linhas = (await resposta.text()).split(/\r?\n/)
    .filter((l) => l.trim() !== "" && !l.trim().startsWith("#"))
  const [cabecalho, ...dados] = linhas
  const colunas = cabecalho.split(separador).map((c) => c.trim())
  return dados.map((linha) => {
    const valores = linha.split(separador)
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
  const todas = (await lerCsv(d.csv))
    .map((l) => {
      const v = numero(l.v_bateria_V)
      const circuito = numero(l.corrente_A)
      const motores = numero(l.corrente_motores_A)
      const i = motores !== null && tensaoMotores !== null && v
        ? (circuito ?? 0) + motores * tensaoMotores / v
        : circuito
      return { ...l, t: numero(l.tempo_min), v, i, motores }
    })
    .filter((l) => l.t !== null)
    .sort((a, b) => a.t - b.t)
  const linhas = todas.filter((l) => l.v !== null)
  const emulada = linhas.some((l) => l.motores !== null)
  const soMotores = todas.filter((l) => l.motores !== null)
  if (linhas.length < 2 && soMotores.length >= 2) {
    montarSoMotores(alvo, soMotores, { capacidade, correntePrevista, minimo, tensaoMotores })
    return
  }
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

/*
 * Sem a LiPo e sem os reguladores, só a corrente dos motores foi medida, com a fonte
 * fixa na tensão dos motores. A corrente que sairia da bateria é estimada pela potência
 * dos motores e do ESP32, com o rendimento de 85% do 4.2, sobre a tensão nominal:
 * Ibat = (Im x Vm + Ilogica x Vlogica) / (Vnominal x rendimento).
 */
function montarSoMotores(alvo, linhas, { capacidade, correntePrevista, minimo, tensaoMotores }) {
  const d = alvo.dataset
  const nominal = numero(d.tensaoNominal)
  const rendimento = numero(d.rendimento)
  const iLogica = numero(d.correnteLogica) ?? 0
  const vLogica = numero(d.tensaoLogica) ?? 0

  alvo.replaceChildren()
  aviso(alvo, `Só os motores foram medidos, com a fonte fixa em ${formatar(tensaoMotores, 2)} V. A tensão da bateria não foi registrada, então a corrente da bateria e a autonomia são estimadas pela potência.`)
  grafico(alvo, "Corrente dos dois motores", linhas.map((l) => ({ x: l.t, y: l.motores })), "A", 3)

  const primeira = linhas[0]
  const ultima = linhas.at(-1)
  /* O cronômetro parte de 0 e a primeira leitura é a do minuto 1. */
  const duracao = ultima.t
  const media = linhas.reduce((s, l) => s + l.motores, 0) / linhas.length
  const variacao = (ultima.motores - primeira.motores) / primeira.motores * 100
  const bateria = nominal && rendimento ? (media * tensaoMotores + iLogica * vLogica) / (nominal * rendimento) : null
  const autonomia = capacidade !== null && bateria ? (0.8 * capacidade) / (bateria * 1000) * 60 : null
  const atende = (ok) => ok === null ? "—" : ok ? "Sim" : "Não"

  const resumo = [
    ["Duração registrada", `${formatar(duracao, 0)} min`, minimo !== null ? `≥ ${formatar(minimo, 0)} min` : "—", atende(minimo !== null ? duracao >= minimo : null)],
    ["Corrente no início e no fim", `${formatar(primeira.motores, 3)} → ${formatar(ultima.motores, 3)} A (${variacao > 0 ? "+" : ""}${formatar(variacao, 0)}%)`, "—", "—"],
    ["Corrente média dos motores", `${formatar(media, 3)} A (${formatar(media / 2 * 1000, 0)} mA por motor)`, "—", "—"],
    ["Potência média dos motores", `${formatar(media * tensaoMotores, 2)} W`, "—", "—"],
    ["Corrente estimada na bateria", `${formatar(bateria, 3)} A`, correntePrevista !== null ? `≤ ${formatar(correntePrevista * 1.3, 2)} A (${formatar(correntePrevista, 2)} A + 30%)` : "—",
      atende(bateria !== null && correntePrevista !== null ? bateria <= correntePrevista * 1.3 : null)],
    ["Autonomia estimada (80% da carga)", `${formatar(autonomia, 0)} min`, "≥ 30 min (3 tentativas de 10 min)", atende(autonomia !== null ? autonomia >= 30 : null)],
  ]
  const titulo = document.createElement("p")
  titulo.innerHTML = "<strong>Resultados calculados a partir do CSV</strong>"
  alvo.append(titulo, tabela(["Grandeza", "Medido", "Esperado", "Atende"], resumo))

  const detalhes = document.createElement("details")
  detalhes.innerHTML = "<summary>Leituras registradas</summary>"
  detalhes.append(tabela(["Tempo (min)", "Motores (A)", "Observação"],
    linhas.map((l) => [formatar(l.t, 0), formatar(l.motores, 3), l.observacao ?? ""])))
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

/*
 * Teste 5.44 do 7.5 (integração): tensão da bateria lida pelo firmware em cada tentativa das corridas,
 * a partir da exportação das execuções feita pela tela (uma linha por tentativa).
 */
function graficoCorridas(alvo, titulo, tipo, series, rotuloY, tentativas, referencias = []) {
  const figura = document.createElement("div")
  figura.className = "energia-grafico"
  const canvas = document.createElement("canvas")
  canvas.setAttribute("role", "img")
  canvas.setAttribute("aria-label", titulo)
  figura.append(canvas)
  alvo.append(figura)

  const tinta = cor("--md-default-fg-color--light", "#666")
  const grade = cor("--md-default-fg-color--lightest", "#ddd")
  const barras = tipo === "bar"
  return new Chart(canvas, {
    type: tipo,
    data: {
      labels: barras ? tentativas.map((l) => l.rotulo) : undefined,
      datasets: series.map((s) => ({
        label: s.rotulo, data: s.dados, borderColor: s.cor, backgroundColor: s.cor,
        borderWidth: barras ? 0 : 2, pointRadius: barras ? 0 : 3, pointHoverRadius: 5, pointHitRadius: 10,
        showLine: s.linha ?? true, tension: 0,
      })),
    },
    options: {
      maintainAspectRatio: false,
      animation: false,
      interaction: { mode: "index", intersect: false },
      plugins: {
        legend: { display: true, position: "bottom", labels: { color: tinta, boxWidth: 12 } },
        title: { display: true, text: titulo, color: tinta, align: "start" },
        tooltip: {
          filter: (item) => item.parsed.y !== null,
          callbacks: {
            title: (itens) => {
              if (!itens.length) return ""
              const l = tentativas[itens[0].dataIndex]
              return `Execução ${l.rotulo}, ${l.hora} (${l.resultado})`
            },
            label: (item) => `${item.dataset.label}: ${formatar(item.parsed.y, 0)} mV`,
          },
        },
        linhasDeReferencia: { linhas: referencias, cor: tinta },
      },
      scales: {
        x: barras
          ? { stacked: true, title: { display: true, text: "Execução (tentativa)", color: tinta }, ticks: { color: tinta }, grid: { display: false } }
          : { type: "linear", title: { display: true, text: "Tempo desde a primeira corrida (min)", color: tinta }, ticks: { color: tinta }, grid: { color: grade } },
        y: {
          stacked: barras,
          title: { display: true, text: rotuloY, color: tinta },
          suggestedMin: referencias.length ? Math.min(...referencias.map((r) => r.valor)) - 50 : undefined,
          ticks: { color: tinta }, grid: { color: grade },
        },
      },
    },
    plugins: [linhasDeReferencia],
  })
}

async function montarCorridas(alvo) {
  const d = alvo.dataset
  const alerta = numero(d.alerta)
  const corte = numero(d.corte)
  const hora = new Intl.DateTimeFormat("pt-BR", { timeZone: "America/Sao_Paulo", hour: "2-digit", minute: "2-digit" })

  const brutas = (await lerCsv(d.csv, ","))
    .map((l) => ({
      ...l, inicio: new Date(l.iniciada_em), fim: new Date(l.encerrada_em), duracao: numero(l.tempo_s),
      vi: numero(l.bateria_inicial), vf: numero(l.bateria_final), pct: numero(l.consumo_bateria_pct),
    }))
    .filter((l) => !Number.isNaN(l.inicio.getTime()))
    .sort((a, b) => a.inicio - b.inicio)
  const comTensao = brutas.filter((l) => l.vi !== null && l.vf !== null)
  if (comTensao.length < 2) {
    aviso(alvo, "Aguardando a exportação das execuções em " + d.csv.replace(/^(\.\.\/)+/, "docs/") + ".")
    return
  }
  const zero = comTensao[0].inicio
  const linhas = comTensao.map((l) => ({
    ...l,
    t: (l.inicio - zero) / 60000,
    queda: l.vi - l.vf,
    concluiu: l.status_tentativa === "success",
    rotulo: l.attempt_index === "1" ? l.numero : `${l.numero}.${l.attempt_index}`,
    hora: hora.format(l.inicio),
    resultado: l.status_tentativa === "success" ? "concluída" : `falha: ${l.motivo_falha || "sem motivo"}`,
  }))
  alvo.replaceChildren()

  const azul = cor("--md-primary-fg-color", "#1f3a5f")
  const laranja = cor("--md-accent-fg-color", "#e8743b")
  graficoCorridas(alvo, "Tensão da bateria lida pelo firmware (mV)", "line", [
    { rotulo: "No início da tentativa", cor: azul, dados: linhas.map((l) => ({ x: l.t, y: l.vi })) },
    { rotulo: "No fim da tentativa", cor: laranja, linha: false, dados: linhas.map((l) => ({ x: l.t, y: l.vf })) },
  ], "Tensão (mV)", linhas, [
    ...(alerta !== null ? [{ valor: alerta, rotulo: `alerta ${formatar(alerta, 0)} mV` }] : []),
    ...(corte !== null ? [{ valor: corte, rotulo: `corte ${formatar(corte, 0)} mV` }] : []),
  ])
  graficoCorridas(alvo, "Queda de tensão durante cada tentativa (mV)", "bar", [
    { rotulo: "Tentativa concluída", cor: azul, dados: linhas.map((l) => l.concluiu ? l.queda : null) },
    { rotulo: "Tentativa com falha (stuck)", cor: laranja, dados: linhas.map((l) => l.concluiu ? null : l.queda) },
  ], "Queda (mV)", linhas)

  const soma = (valores) => valores.reduce((s, x) => s + x, 0)
  const media = (valores) => valores.length ? soma(valores) / valores.length : null
  const grupo = (lista) => {
    const tempo = soma(lista.map((l) => l.duracao ?? 0))
    return {
      n: lista.length, tempo,
      queda: media(lista.map((l) => l.queda)),
      taxa: tempo ? soma(lista.map((l) => l.queda)) / tempo : null,
    }
  }
  const boas = grupo(linhas.filter((l) => l.concluiu))
  const ruins = grupo(linhas.filter((l) => !l.concluiu))
  const execucoes = new Map(linhas.map((l) => [l.numero, l.status_execucao]))
  const concluidas = [...execucoes.values()].filter((s) => s === "concluida").length
  const primeira = linhas[0]
  const ultima = linhas.at(-1)
  const sessao = (ultima.fim - zero) / 60000
  const menor = Math.min(...linhas.map((l) => l.vf))
  const emAlerta = alerta !== null ? linhas.filter((l) => l.vi < alerta) : []
  const recuperacao = media(linhas.slice(1).map((l, k) => l.vi - linhas[k].vf))
  const subiram = linhas.filter((l) => l.queda < 0)
  const atende = (ok) => ok === null ? "—" : ok ? "Sim" : "Não"

  const resumo = [
    ["Duração da sessão", `${formatar(sessao, 0)} min (${hora.format(zero)} às ${hora.format(ultima.fim)})`, "—", "—"],
    ["Execuções e tentativas", `${execucoes.size} execuções (${concluidas} concluídas) em ${linhas.length} tentativas`, "—", "—"],
    ["Tentativas com a tensão no início e no fim", `${linhas.length} de ${brutas.length}`, "todas", atende(linhas.length === brutas.length)],
    ["Tempo com o robô andando", `${formatar(boas.tempo + ruins.tempo, 0)} s (${formatar((boas.tempo + ruins.tempo) / 60, 1)} min)`, "—", "—"],
    ["Tensão na primeira e na última leitura", `${formatar(primeira.vi, 0)} → ${formatar(ultima.vf, 0)} mV`, "—", "—"],
    ["Menor tensão registrada", `${formatar(menor, 0)} mV`, corte !== null ? `acima do corte de ${formatar(corte, 0)} mV` : "—", atende(corte !== null ? menor > corte : null)],
    ["Tentativas iniciadas abaixo do alerta", alerta !== null ? `${emAlerta.length} de ${linhas.length}${emAlerta.length ? `, a partir da execução ${emAlerta[0].rotulo}` : ""}` : "—", "—", "—"],
    ["Queda de tensão média por tentativa concluída", `${formatar(boas.queda, 1)} mV (${formatar(boas.taxa, 2)} mV/s) em ${boas.n} tentativas`, "—", "—"],
    ["Queda de tensão média por tentativa com falha", `${formatar(ruins.queda, 1)} mV (${formatar(ruins.taxa, 2)} mV/s) em ${ruins.n} tentativas`, "—", "—"],
    ["Recuperação média entre uma tentativa e a seguinte", `${formatar(recuperacao, 1)} mV`, "—", "—"],
    ["Tentativas em que a leitura subiu", `${subiram.length} (até ${formatar(Math.max(0, ...subiram.map((l) => -l.queda)), 0)} mV)`, "—", "—"],
  ]
  const titulo = document.createElement("p")
  titulo.innerHTML = "<strong>Resultados calculados a partir da exportação</strong>"
  alvo.append(titulo, tabela(["Grandeza", "Medido", "Esperado", "Atende"], resumo))

  const detalhes = document.createElement("details")
  detalhes.innerHTML = "<summary>Tentativas registradas</summary>"
  detalhes.append(tabela(["Execução", "Início", "Resultado", "Duração (s)", "Passos", "Início (mV)", "Fim (mV)", "Queda (mV)", "Consumo na tela (%)"],
    linhas.map((l) => [l.rotulo, l.hora, l.resultado, formatar(l.duracao, 1), l.passos, formatar(l.vi, 0), formatar(l.vf, 0),
      formatar(l.queda, 0), formatar(l.pct, 2)])))
  alvo.append(detalhes)
}

document$.subscribe(() => {
  document.querySelectorAll(".energia-corridas").forEach((alvo) =>
    montarCorridas(alvo).catch((erro) => aviso(alvo, `Erro ao montar o gráfico: ${erro.message}`)))
  document.querySelectorAll(".energia-descarga").forEach((alvo) =>
    montarDescarga(alvo).catch((erro) => aviso(alvo, `Erro ao montar o gráfico: ${erro.message}`)))
  document.querySelectorAll(".energia-consumo").forEach((alvo) =>
    montarConsumo(alvo).catch((erro) => aviso(alvo, `Erro ao montar a tabela: ${erro.message}`)))
})
