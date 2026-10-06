import type { EventoExecucao } from './tipos'

export class ErroApi extends Error {
  readonly status: number
  readonly corpo: unknown

  constructor(status: number, corpo: unknown) {
    super(`Erro ${status} na API`)
    this.name = 'ErroApi'
    this.status = status
    this.corpo = corpo
  }
}

/** Ponto único de HTTP e SSE da View (diagrama de classes MVC, 4.4). */
export class ClienteAPI {
  private readonly baseUrl: string

  constructor(baseUrl: string) {
    this.baseUrl = baseUrl.replace(/\/$/, '')
  }

  get<T>(caminho: string, sinal?: AbortSignal): Promise<T> {
    return this.requisitar<T>('GET', caminho, undefined, sinal)
  }

  post<T>(caminho: string, corpo?: unknown): Promise<T> {
    return this.requisitar<T>('POST', caminho, corpo)
  }

  /** Assina o stream SSE da execução. Devolve a função que encerra a assinatura. */
  assinar(execucaoId: string, aoReceber: (evento: EventoExecucao) => void): () => void {
    if (typeof EventSource === 'undefined') return () => {}

    const eventos = new EventSource(this.url(`/execucoes/${execucaoId}/stream`))
    eventos.onmessage = (mensagem: MessageEvent<string>) => {
      aoReceber(JSON.parse(mensagem.data) as EventoExecucao)
    }
    return () => eventos.close()
  }

  private url(caminho: string): string {
    const base = this.baseUrl.startsWith('http') ? this.baseUrl : `${window.location.origin}${this.baseUrl}`
    return `${base}${caminho}`
  }

  private async requisitar<T>(metodo: string, caminho: string, corpo?: unknown, sinal?: AbortSignal): Promise<T> {
    const resposta = await fetch(this.url(caminho), {
      method: metodo,
      headers: corpo === undefined ? undefined : { 'Content-Type': 'application/json' },
      body: corpo === undefined ? undefined : JSON.stringify(corpo),
      signal: sinal,
    })
    const texto = await resposta.text()
    const dados: unknown = texto ? JSON.parse(texto) : null
    if (!resposta.ok) throw new ErroApi(resposta.status, dados)
    return dados as T
  }
}

export const api = new ClienteAPI(import.meta.env.VITE_API_URL ?? '/api')
