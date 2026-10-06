import { screen } from '@testing-library/react'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import { ClienteAPI, ErroApi, ErroSemConexao } from '../api/clienteApi'
import { servidor } from '../mocks/servidor'
import { renderizarRota } from './renderizar'

const cliente = new ClienteAPI('http://teste/api')

describe('ClienteAPI sem comunicação com o servidor (FRONT-01)', () => {
  it('backend desligado (a requisição nem chega) vira ErroSemConexao', async () => {
    servidor.use(http.get('*/api/labirintos', () => HttpResponse.error()))
    await expect(cliente.get('/labirintos')).rejects.toBeInstanceOf(ErroSemConexao)
  })

  it('502 do proxy (backend fora do ar) vira ErroSemConexao', async () => {
    servidor.use(http.get('*/api/labirintos', () => new HttpResponse(null, { status: 502 })))
    await expect(cliente.get('/labirintos')).rejects.toBeInstanceOf(ErroSemConexao)
  })

  it('erro com corpo que não é JSON vira ErroApi com o texto, sem quebrar', async () => {
    servidor.use(http.get('*/api/labirintos', () => new HttpResponse('Internal Server Error', { status: 500 })))
    const erro = await cliente.get('/labirintos').catch((e: unknown) => e)
    expect(erro).toBeInstanceOf(ErroApi)
    expect((erro as ErroApi).corpo).toBe('Internal Server Error')
  })
})

describe('telas com o servidor fora do ar mostram aviso, não tela em branco', () => {
  it('Início', async () => {
    servidor.use(http.get('*/api/execucoes/atual', () => HttpResponse.error()))
    renderizarRota('/')
    expect(await screen.findByText(/Sem comunicação com o servidor/)).toBeInTheDocument()
  })

  it('Histórico', async () => {
    servidor.use(http.get('*/api/execucoes', () => new HttpResponse(null, { status: 502 })))
    renderizarRota('/execucoes')
    expect(await screen.findByText(/Sem comunicação com o servidor/)).toBeInTheDocument()
  })
})
