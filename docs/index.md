<div class="hero" markdown>

<div class="hero__copy" markdown>

<p class="eyebrow">Projeto Integrador · 2026.2</p>

# Ratatouille

<p class="hero__subtitle">Um Micromouse autônomo para explorar, mapear e solucionar labirintos — com telemetria para entender cada tentativa.</p>

<div class="hero__actions" markdown>

[:material-rocket-launch: Começar](1 - TAP.md){ .md-button .md-button--primary }
[:material-vector-polyline: Arquitetura proposta](4.4 - Projeto conceitual de software.md){ .md-button }
[:material-account-group: Equipe](1 - TAP.md#membros-da-equipe){ .md-button }

</div>

</div>

<div class="hero__visual">
  <div class="maze-art" role="img" aria-label="Ilustração abstrata de um robô percorrendo um labirinto">
    <div class="maze-art__grid">
      <div class="maze-art__robot"></div>
      <div class="maze-art__goal">◎</div>
    </div>
  </div>
</div>

</div>

O **Ratatouille** é um projeto de robótica móvel do Grupo 01. A solução prevista combina um robô autônomo, subsistemas de estrutura, energia e eletrônica, firmware de navegação e um sistema web de telemetria com histórico das execuções.

!!! note "O que está documentado aqui"
    Esta documentação registra o planejamento e as decisões propostas para o projeto. Onde a equipe ainda não registrou uma implementação, um resultado ou uma escolha técnica, a pendência é mantida explícita.

## O projeto em números

<div class="stat-grid" markdown>

<div class="stat" markdown>
<span class="stat__value">16,5 cm</span>
<span class="stat__label">limite de comprimento ou largura do robô</span>
</div>

<div class="stat" markdown>
<span class="stat__value">18 cm</span>
<span class="stat__label">lado de cada célula do labirinto</span>
</div>

<div class="stat" markdown>
<span class="stat__value">10 min</span>
<span class="stat__label">limite por desafio durante os testes</span>
</div>

<div class="stat" markdown>
<span class="stat__value">4×4 · 8×4 · 12×4</span>
<span class="stat__label">modelos de labirinto previstos</span>
</div>

</div>

## Capacidades previstas

<div class="feature-grid" markdown>

<div class="feature-card" markdown>
<span class="feature-card__icon">:material-radar:</span>

### Navegação autônoma

O Micromouse deve detectar paredes e obstáculos, mapear o ambiente e escolher seu trajeto sem controle externo.

[Ver requisitos](2 - Requisitos.md)
</div>

<div class="feature-card" markdown>
<span class="feature-card__icon">:material-chart-timeline-variant:</span>

### Telemetria de cada tentativa

O sistema web deve receber e exibir dados como trajeto, bateria, velocidade média, tempo e status da execução.

[Ver escopo](1 - TAP.md#escopo-do-produto)
</div>

<div class="feature-card" markdown>
<span class="feature-card__icon">:material-puzzle-outline:</span>

### Engenharia integrada

Estrutura, energia, hardware e software são tratados como subsistemas que precisam funcionar em conjunto.

[Ver EAP](3 - EAP.md)
</div>

<div class="feature-card" markdown>
<span class="feature-card__icon">:material-database-outline:</span>

### Histórico para análise

Os dados das execuções devem ser armazenados em banco de dados para permitir consultas e comparação de desempenho.

[Ver requisitos de software](4.4 - Projeto conceitual de software.md)
</div>

</div>

## Como o sistema funciona

<div class="process" markdown>

<div class="process__step" markdown>
<span class="process__number">01 · PERCEPÇÃO</span>

### Ler o ambiente

Sensores detectam paredes, obstáculos e condições relevantes do labirinto.
</div>

<div class="process__step" markdown>
<span class="process__number">02 · NAVEGAÇÃO</span>

### Decidir o caminho

O firmware deve localizar o robô, descobrir paredes e conduzir o mapeamento do labirinto.
</div>

<div class="process__step" markdown>
<span class="process__number">03 · MOVIMENTO</span>

### Executar a rota

Motores e estrutura trabalham juntos para avançar, fazer curvas e chegar ao objetivo.
</div>

<div class="process__step" markdown>
<span class="process__number">04 · TELEMETRIA</span>

### Registrar a tentativa

Dados de navegação são enviados ao sistema web para acompanhamento e análise posterior.
</div>

</div>

## Acesso rápido

<div class="quick-links" markdown>

<div class="quick-link" markdown>
### :material-clipboard-text-outline: Planejamento
<p>Escopo, objetivos, stakeholders, recursos e critérios de aceite do projeto.</p>

[Abrir o TAP](1 - TAP.md)
</div>

<div class="quick-link" markdown>
### :material-cog-outline: Projeto conceitual
<p>Decisões e orientações para estrutura, energia, hardware e software.</p>

[Explorar subsistemas](4.1 - Projeto conceitual de estruturas.md)
</div>

<div class="quick-link" markdown>
### :material-clipboard-check-outline: Validação
<p>Planos de testes isolados, integração e avaliação do desempenho da equipe.</p>

[Ver testes](7.1 - Testes de estrutura.md)
</div>

</div>

!!! warning "Pendências registradas no repositório"
    As páginas de projeto conceitual, cronograma, orçamento, testes e encerramento ainda contêm instruções, tabelas vazias ou campos a preencher. Também não há implementação versionada em `src/`, apenas READMEs de organização das áreas. Essas lacunas foram mantidas para não criar informações que ainda não foram registradas pela equipe.

## Links do projeto

- [Apresentação do tema](https://docs.google.com/presentation/d/1hRx0g9mpVGQbTv44gLEoAyf4b4kKM4vnrABTnG8HlUk/edit?usp=sharing)
- [Plano de ensino](https://docs.google.com/presentation/d/1rfWSq6o3oxLBDlOsa74zmkzughcoZcS-lg7VR7i0qEI/edit?usp=sharing)
- [Repositório no GitHub](https://github.com/PI1-2026-2/2026_2_PI1_Grupo01_Juliana)
