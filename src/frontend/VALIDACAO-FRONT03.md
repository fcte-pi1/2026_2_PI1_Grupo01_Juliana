# Validação integrada do FRONT-03

Referências: [FRONT-03 #132](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/132), [revisão #208](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/pull/208), [unidades #211](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/211) e [ADR-016](../../docs/decisoes/016-bateria-mv-e-porcentagem.md).

## Backend do projeto

Preparar API e PostgreSQL conforme o [README do backend](../backend/README.md), incluindo as migrações. Confirmar `GET http://localhost:8000/back-health` antes de iniciar a validação. O simulador usa o banco de desenvolvimento; testes de pytest exigem um banco separado.

No PowerShell, em `src/frontend`:

```powershell
$env:VITE_API_MOCK = 'false'
npm.cmd run dev
```

Abrir `http://localhost:5173/` sem `cenario`. O botão **Mock** deve estar ausente. O Vite encaminha `/api/execucoes` para `/execucoes` no backend, inclusive `/stream`; `VITE_API_URL` permanece no padrão `/api`.

## Corrida com o simulador existente

Na interface, selecionar 4x4 e clicar **Nova execução**, abrindo a tentativa antes de enviar a telemetria. Com o ambiente do [simulador oficial](../simulador-telemetria/README.md) já preparado, em outro terminal, em `src/simulador-telemetria`:

```powershell
uv run python -m simulador roteiros/sucesso-4x4.json --saida http --url http://localhost:8000 --acelerar 0.25
```

O simulador usa os componentes do contrato do próprio backend e envia health-check, telemetria, passos e sucesso ao `POST /telemetria` real. Ele não cria a execução. O fator 0,25 alonga a reprodução para permitir o recarregamento durante a corrida. Conferir:

1. Health-check → em execução → concluída, na mesma execução. No DevTools, `/api/execucoes/{id}/stream` deve responder `text/event-stream`, com os eventos nomeados correspondentes ao fluxo.
2. Comparar os valores de uma mensagem `telemetria` com o card e a tabela: bateria em mV, velocidade em mm/s e percentual recebido sem arredondar. A velocidade média permanece em m/s. No health-check, bateria em mV e ToF em mm.
3. Recarregar durante a corrida. Confirmar o mesmo ID/tentativa, dados restaurados pelo GET e continuidade das atualizações pelo SSE. O percentual da leitura aguarda o próximo SSE, pois não é fornecido pelo GET.
4. Ao terminar, confirmar dados finais, consumo em mV e percentual e variação contra o melhor tempo. Sem comparação disponível, o campo permanece pendente. A URL deve conservar `execucao=<id>`: recarregar mantém o resultado pelo GET de detalhe. Abrir histórico → detalhe e comparar com o GET. Usar **Voltar ao início** para preparar outra corrida; criar uma nova execução deve remover a seleção anterior da URL, inclusive após recarregar.
5. Para exercitar falha, abrir outra execução na interface e reproduzir `roteiros/falha-componente.json` com as mesmas opções. Conferir falha e célula recebidas, ação Retomar com checklist e abertura de outra tentativa na mesma execução. O novo health-check depende de novas mensagens do simulador/robô; a interface não o aprova sozinha.

O script auxiliar `src/backend/scripts/simular_ponte_telemetria.py` contém nove componentes no health-check, incluindo `tof_frontal_esq` e `tof_frontal_dir`, enquanto o contrato e o modelo de persistência atuais aceitam oito, com `tof_frontal`. Essa divergência é do backend e não foi alterada nesta entrega. Por isso, o roteiro acima usa o simulador oficial, que importa o contrato existente.

## Latência RNF-B02

Registrar uma amostra identificável do simulador, o instante de persistência no backend e o instante em que o mesmo valor apareceu no navegador. Usar relógios sincronizados e conservar evidências e valores medidos. O aceite exige no máximo 2 s entre o registro e a exibição; `enviado_em` do robô, isoladamente, não informa o instante do COMMIT. O teste automatizado com transporte controlado e a demonstração MSW não substituem essa medição.

## Demonstração local

```powershell
$env:VITE_API_MOCK = 'true'
npm.cmd run dev
```

Começar em `/?cenario=01-inicio`, escolher 4x4 e clicar **Nova execução**. Acompanhar health-check → corrida → conclusão, ou encerrar durante a corrida, preencher o motivo, retomar pelo checklist e acompanhar a tentativa seguinte. Recarregar no meio preserva a execução da aba. Depois de encerrada, o resultado permanece selecionado por `execucao` na URL e pode ser reaberto pelo seletor Mock, histórico ou detalhe; `/execucoes/em-andamento` retorna 204, como no backend.

Os cenários individuais continuam disponíveis pelo painel Mock para inspeção de estados específicos, como perda de comunicação e limite de tentativas. O cenário estático 12 preserva os dados do protótipo, cujas datas das três tentativas não permitem reconstruir uma cronologia sem sobreposição; ele não deve servir como evidência dos cronômetros. O fluxo iniciado por **Nova execução** mantém suas próprias datas. Essa demonstração reproduz os exemplos existentes e não comprova o funcionamento do backend real.

## Evidências desta alteração

- Integração do código da BACK-07/BACK-04: incluída da `dev`, sem implementação própria no backend.
- FRONT-04 [#205](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/pull/205), já mesclado na `dev`: incluído na integração local, preservando paredes, falhas, retomadas e o resultado `cancelada` nos mocks. Resumo e mapa acompanham o eixo informado; a posição reconciliada prevalece sobre passos antigos, inclusive após uma falha e recarregamento.
- Verificação automatizada: 146 testes passaram, incluindo os cenários de integração FRONT-03/FRONT-04, unidades, eventos repetidos e restauração pelo GET.
- API/PostgreSQL locais indisponíveis: a tentativa de executar o simulador terminou em conexão recusada no `/back-health`, antes de criar dados.
- RNF-B02 com backend real e restauração durante corrida real: **pendentes de execução com API e banco ativos**.

## Continuidade no FRONT-05

O [FRONT-05 #122](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/122) consolida o componente `ControleTentativas` e os modais de encerramento/retomada: seleção obrigatória do motivo, observação limitada a 100 caracteres, ações conforme as tentativas disponíveis e bloqueio de cliques repetidos enquanto o pedido estiver pendente. O fluxo já conectado permite exercitar essas ações; os testes específicos do componente e dos modais pertencem a essa entrega. O aceite de SSE, unidades e restauração com o backend real continua sendo uma pendência do FRONT-03 até a execução do roteiro acima.
