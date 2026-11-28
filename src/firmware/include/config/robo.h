#ifndef CONFIG_ROBO_H
#define CONFIG_ROBO_H

/*
 * Constantes físicas do robô e da pista (ARQ-09, #111).
 *
 * Cada valor indica a fonte. Os marcados como PROVISÓRIO ainda não têm fonte
 * nas outras áreas e precisam ser confirmados (datasheet, CAD ou bancada).
 *
 * Fontes:
 *   [TAP]  docs/1 - TAP.md
 *   [EST]  docs/4.1 - Projeto conceitual de estruturas.md
 *   [ENE]  docs/4.2 - Projeto conceitual de energia.md
 *   [HW]   docs/4.3 - Projeto conceitual de hardware.md e hw/SCH_Micromouse_2026-09-26.pdf
 *   [MED]  medido no robô montado e no labirinto da equipe em 24/11/2026 (régua; ver docs/decisoes/024)
 */

/* Botão BOOT */
#define BOTAO_DEBOUNCE_MS           30      /* o contato precisa ficar estável por este tempo */
#define BOTAO_LONGO_MS              1500    /* a partir daqui o toque é longo; PROVISÓRIO, confirmar com a equipe */

/* Pista — [TAP] células de 18 cm; [MED] paredes de 1,5 cm (o TAP diz 1,2 cm) */
#define CELULA_MM                   180
/* Muda de pista para pista: 15 na 1ª pista de testes, 18 na 2ª, 12 na da faculdade (Tema_PI1.pdf). */
#ifndef PAREDE_ESPESSURA_MM
#define PAREDE_ESPESSURA_MM         15
#endif
#define CELULA_LIVRE_MEIA_MM        ((CELULA_MM - PAREDE_ESPESSURA_MM) / 2.0f) /* do centro da célula à face da parede: 82,5 */
#define LABIRINTO_MAX_CELULAS       12   /* maior lado possível (12x4) */
/* Só para testes: o firmware lê o tipo no DIP switch (lib/dip). Usar onde nav.h já foi incluído. */
#define ROBO_TIPO_LABIRINTO         NAV_TIPO_4X4

/* Rodas e tração */
/*
 * [MED] pelo avanço, na bancada de calibração (25/11/2026): com 34 mm (o nominal da roda 34 x 6,5) o robô andou
 * 170 e 175 mm para 176 e 182 mm contados pelos encoders, e 350 e 350 mm para 361 e 358: 3,2 % a menos, igual nas
 * duas rodas e sem patinar. O pneu assentado no chão roda com um diâmetro efetivo menor que o nominal.
 */
#define RODA_DIAMETRO_MM            33.0f
/*
 * [MED] pelo giro, na bancada de calibração (25/11/2026), em chão livre: 10 giros de 90° no lugar somaram 3659 e
 * 3689 pulsos e o robô girou ~890° (faltaram ~10° para as 2,5 voltas); um giro só deu 89° com 368 e 364 pulsos.
 * Com a roda de 33 mm, isso dá 85,4 mm entre os pontos de apoio. Na régua: 90 mm por fora dos pneus de 6,5 mm, ou
 * ~83,5 de centro a centro. Era 87 (régua, com a roda de 34); o CAD dizia 92,5.
 */
/* Depende do piso: onde a roda patina mais no giro, o valor efetivo cresce (o robô gira de menos). */
#ifndef BITOLA_MM
#define BITOLA_MM                   85.5f
#endif
#define VELOCIDADE_MAX_MM_S         850.0f  /* [EST] máxima efetiva do N20 de 600 RPM com a roda de 34 mm */
/*
 * Velocidade de operação: 400 no projeto, 250 em 24/11/2026 e 200 em 25/11/2026 (o robô ainda passava rápido demais
 * pela pista). A 200 mm/s o ToF lê a cada 10 mm.
 * 200 é o piso no simulador: a 180 e a 150 o avanço FALHA com a zona morta alta do motor
 * (test_zona_morta_alta_nao_faz_o_avanco_falhar), porque a potência de cruzeiro fica abaixo da que tira a roda do
 * lugar (MOV_ZONA_MORTA) e ela anda aos trancos. Para andar mais devagar que isso, é preciso mexer no controle.
 * Para ensaiar outro valor sem editar o arquivo: -D VELOCIDADE_CRUZEIRO_MM_S=220.0f no build_flags.
 */
#ifndef VELOCIDADE_CRUZEIRO_MM_S
#define VELOCIDADE_CRUZEIRO_MM_S    200.0f
#endif

/*
 * Geometria do robô montado [MED]. O CAD do docs/4.1 punha o eixo no meio de um chassi de 92 x 103 mm;
 * no robô o eixo das rodas fica perto da traseira. Origem no ponto médio entre as rodas (o centro do giro
 * no lugar), x para a frente e y para a esquerda.
 *
 * Consequências (docs/decisoes/024):
 *   - parado com o chassi centrado na célula, o eixo fica EIXO_RECUO_MM atrás do centro da célula;
 *   - a quina da frente gira num raio de ~100 mm, mais que os 82,5 mm do centro à parede: o giro de 90°
 *     precisa de espaço à frente, e o de 180° não cabe no lugar num corredor (a execução faz uma manobra).
 */
#define EIXO_ATE_FRENTE_MM          90.0f   /* [MED] do eixo das rodas à frente do robô (régua, 26/11/2026; era 87,5) */
#define EIXO_ATE_TRAS_MM            25.0f   /* do eixo à traseira */
#define MEIA_LARGURA_ESQ_MM         50.0f   /* do meio à lateral esquerda, contando os fios que sobram */
#define MEIA_LARGURA_DIR_MM         47.5f   /* do meio à lateral direita */
#define EIXO_RECUO_MM               ((EIXO_ATE_FRENTE_MM - EIXO_ATE_TRAS_MM) / 2.0f) /* 31,25 */

/*
 * Primitivas de movimento (FIRM-05, #135).
 * Valores ajustados no mundo simulado; os da pista são ajustados no FIRM-10 (#141).
 */
#define MOV_PERIODO_MS              5       /* 200 Hz, o mesmo laço do PID do FIRM-04 */
#ifndef MOV_ACELERACAO_MM_S2
#define MOV_ACELERACAO_MM_S2        1000.0f /* rampa: chega a 200 mm/s em 0,2 s e 20 mm; menos patinagem */
#endif
/* Giro no lugar: por padrão igual ao arco; o arco não pode ser mais lento (a roda de dentro para). */
#ifndef MOV_VELOCIDADE_GIRO_LUGAR_MM_S
#define MOV_VELOCIDADE_GIRO_LUGAR_MM_S MOV_VELOCIDADE_GIRO_MM_S
#endif
#ifndef MOV_VELOCIDADE_GIRO_MM_S
#define MOV_VELOCIDADE_GIRO_MM_S    140.0f  /* 150 -> 120 em 25/11 -> 140 em 26/11/2026: a 120 a roda de dentro do arco ficava a ~63 mm/s e parava; com 150 e o raio de 120 o simulador encosta no labirinto 4x4-03 */
#endif
#define MOV_ATRASO_MOTOR_S          0.05f   /* constante de tempo da roda; medir na bancada (FIRM-04) */
#define MOV_KP_POSICAO              1.5f    /* potência por pulso de erro de cada roda */
#define MOV_TOLERANCIA_PULSOS       6       /* erro aceito no fim do giro, por roda (~1°) */
#define MOV_TOLERANCIA_AVANCO_PULSOS 8      /* no avanço o atrito do motor deixa a roda oscilar: ~1,5 mm */
#define MOV_KI_POSICAO              25.0f   /* potência por pulso de erro e por segundo: vence o atrito parado no fim do movimento */
#define MOV_INTEGRAL_MAX            500.0f  /* limite da parcela integral, em potência (anti-windup) */
/*
 * Zona morta do N20 no chão: abaixo dessa potência a roda parada não sai do lugar. Na pista (24/11/2026) o robô
 * parou a ~1 cm do fim de uma célula e empurrou até o tempo limite. Roda parada longe do alvo recebe pelo menos isto.
 */
#define MOV_ZONA_MORTA              250.0f
#define MOV_INTEGRAL_SOLTA          0.8f    /* fator por ciclo da integral com a roda andando: desgrudou, a força extra some */
/*
 * Perto basta: com a trajetória no fim e as duas rodas paradas há MOV_PERTO_PARADO_MS, um erro de até isto conta
 * como chegada. O que faltou entra no movimento seguinte (resíduo). Com zona morta alta, cada vez que a roda
 * desgruda ela anda mais que a tolerância fina, e sem isto o movimento esgotaria o tempo e falharia.
 */
#define MOV_PERTO_AVANCO_PULSOS     32      /* ~6 mm */
#define MOV_PERTO_GIRO_PULSOS       12      /* ~2 mm de roda, ~2,5° */
#define MOV_PERTO_PARADO_MS         150
/* No tempo limite, um erro de até 2,5 x o "perto" (~15 mm no avanço, ~6° no giro) ainda conta como chegada. */
#define MOV_ASSENTAR_MS             50      /* tempo dentro da tolerância para concluir */
#define MOV_TEMPO_LIMITE_MS         3000    /* sem concluir nesse tempo, o movimento falha */
#define MAQ_HC_VALIDADE_MS          600000UL /* o health-check aprovado vale por 10 min: depois disso a largada exige um novo */
#define MOV_FREIO_MS                250     /* tempo que as rodas ficam freadas na parada de emergência, antes de soltar a ponte H */
#define MOV_TRAVADO_MS              200     /* roda sem contar pulsos com potência alta: o movimento falha */
#define MOV_TRAVADO_POTENCIA        600     /* potência a partir da qual a roda parada conta como travada */

/*
 * Centralização no corredor (FIRM-10). O avanço estima a posição lateral do eixo e o rumo do robô em relação
 * ao corredor: a odometria prevê, e cada leitura das paredes laterais corrige (o quanto a lateral muda ao
 * longo do avanço mostra o rumo). O rumo pedido leva o eixo ao meio, e o robô para alinhado com o corredor.
 * A execução informa onde o eixo ficou depois de cada giro (o eixo atrás do centro desloca o robô de lado)
 * e desconta o rumo estimado no giro seguinte.
 */
#define MOV_CENTRO_GANHO_RAD_POR_MM 0.02f   /* rumo pedido por mm fora do centro: o desvio cai à metade em ~35 mm */
#define MOV_CENTRO_RUMO_MAX_RAD     0.15f   /* ~8,6°: o nariz não vai para cima da parede */
#define MOV_CENTRO_FUSAO            0.4f    /* peso de cada leitura lateral nova contra a estimativa da odometria */
#ifndef MOV_RUMO_FUSAO
#define MOV_RUMO_FUSAO              0.03f   /* quanto da diferença entre medida e estimativa, por mm, vira rumo */
#endif
/*
 * Leituras laterais que não servem para centralizar: acima de MOV_LATERAL_MAX_MM o robô estaria com o eixo a
 * mais de ~40 mm do meio (dentro da outra parede); a mediana dá valores assim quando o feixe passa por um
 * poste ou pela borda de uma abertura. E uma leitura que discorda da estimativa mais que o portão é descartada.
 */
#define MOV_LATERAL_MAX_MM          85
#define MOV_CENTRO_PORTAO_MM        20.0f
/* Ré reta: mede o rumo pela parede do lado (ver medir_rumo_na_re). Só em ré longa e com leitura bastante. */
#ifndef MOV_RE_RUMO_MIN_MM
#define MOV_RE_RUMO_MIN_MM 90.0f
#endif
#ifndef MOV_RE_RUMO_LEITURAS_MM
#define MOV_RE_RUMO_LEITURAS_MM 60.0f
#endif
/* Torto assim ou mais no começo do arco, o robô se endireita no lugar antes (ver comecar_etapa). */
#ifndef ARCO_ALINHAR_MIN_GRAUS
#define ARCO_ALINHAR_MIN_GRAUS 5.0f
#endif
#define MOV_RE_RUMO_MAX_RAD 0.35f
#define MOV_RE_RUMO_LONGA_MM 150.0f
#define MOV_RE_RUMO_CURTA_MIN_RAD 0.12f
#define MOV_CENTRO_SALTO_MM         8.0f    /* entre duas leituras seguidas (50 ms): mais que isto é borda de parede */
#define MOV_CENTRO_PORTAO_LEITURAS  3       /* tantas leituras seguidas fora do portão, e a estimativa é que está errada */
#define MOV_RUMO_MAX_RAD            0.3f    /* ~17°: limite da estimativa do rumo */
/*
 * Deriva: uma roda um pouco maior que a outra entorta o robô sempre para o mesmo lado, sem o encoder ver.
 * A cada avanço com paredes, o que o rumo precisou ser corrigido por mm andado vira deriva aprendida, que os
 * avanços seguintes já descontam.
 */
#ifndef MOV_DERIVA_APRENDIZADO
#define MOV_DERIVA_APRENDIZADO      0.7f
#endif
#define MOV_DERIVA_MAX_RAD_POR_MM   0.0005f /* ~2,9° a cada 100 mm: uma roda ~2,5% maior */
#define MOV_CENTRO_FIM_MM           40.0f   /* nos últimos mm o rumo pedido cai a zero: o robô para reto */
#define MOV_RUMO_PASSO_PULSOS       2.0f    /* quanto o desvio entre as rodas muda por ciclo, no máximo */

#define MOV_GANHO_FRONTAL           0.5f    /* 0 a 1: quanto o fim do avanço segue a parede da frente */
#define MOV_FRONTAL_JANELA_MM       40.0f   /* só corrige pela parede se ela estiver a isto do fim previsto */
#define MOV_FRONTAL_MIN_MM          (TOF_MIN_CONFIAVEL_MM + 10) /* folga do ruído: perto disso a leitura pode ser comprimida */
/*
 * A correção frontal só pode encurtar o avanço (e esticar no máximo isto): na pista, um frontal que lia a parede
 * mais longe do que ela estava fez o robô passar do ponto e encostar nela.
 */
#define MOV_FRONTAL_EXTENSAO_MM     5.0f
/*
 * Encostou na parede no fim do avanço de célula: a roda para de avançar, a até MOV_PAREDE_FIM_MM do fim previsto,
 * por MOV_PAREDE_PARADO_MS. Conta como chegada, com o nariz na parede (posição conhecida), em vez de empurrar até
 * o tempo limite e falhar.
 */
#define MOV_PAREDE_FIM_MM           50.0f
#define MOV_PAREDE_PARADO_MS        300
#define MOV_PAREDE_POTENCIA         450.0f  /* só é parede se as duas rodas empurram com isto e não andam (não é zona morta) */
/*
 * Ajuste pela parede da frente, parado na célula [MED]. Na corrida de 26/11/2026 o robô chegou várias vezes 4 a
 * 12 cm antes do repouso (frontal em 75, 91, 86, 65 e 148 mm, para os 30 a 45 que ele mostra no repouso): a
 * meia-volta e o arco que enrosca deixam o robô para trás, e a correção frontal do avanço só encurta. Partindo
 * daí, a curva seguinte sai colada na parede de dentro ou enrosca, e a chegada foi dada com o robô fora da célula.
 * No repouso, uma parede à frente entre MIN e MAX só pode ser a desta célula vista de trás (a da célula seguinte
 * estaria a mais de 200 mm): o robô avança até o frontal mostrar ALVO.
 */
#define AJUSTE_FRENTE_MIN_MM        60
#define AJUSTE_FRENTE_MAX_MM        170
#define AJUSTE_FRENTE_ALVO_MM       40
#define MOV_FRENTE_SOBRA_MM         10      /* distância mínima até a parede à frente que a parada de emergência garante */
#define MOV_FRENTE_FIM_MM           35.0f   /* parada de emergência a isto do fim do avanço conta como chegada, não falha */
#define MOV_TOF_ATRASO_S            0.075f  /* atraso do ToF frontal até o movimento: meia amostra de 50 ms mais a mediana de 3 a 20 Hz */

/* NVS: a flash aguenta cerca de 100 mil apagamentos por setor. */
#define MEM_GRAVACOES_MAX           600     /* gravações do mapa por inicialização; um robô que oscila entre células não gasta a flash */

/* Encoder — [MED] 10 voltas marcadas, girando à mão: 5759 e 5743 (esq.), 5716 e 5752 (dir.) */
#define ENCODER_PULSOS_POR_VOLTA    574     /* com 640 o robô andava 18 a 20 cm por célula e girava ~107° por 90° */

/* Derivadas: o controle, o mundo simulado e os testes usam as mesmas. */
#define PI_F                        3.14159265f
#define MM_POR_PULSO                (PI_F * RODA_DIAMETRO_MM / ENCODER_PULSOS_POR_VOLTA)

/*
 * Motores — [ENE] 4.2 v1.3, Tabelas 7 e 8.
 * O limite de PWM não é fixo: potência máxima = MOTOR_POTENCIA_MAX x 6000 / V_bateria_mV,
 * limitada a 100 %. Dá 71 % com 8,4 V, 81 % com 7,4 V e 100 % com as pilhas (≤ 6 V).
 */
#define MOTOR_TENSAO_NOMINAL_MV     6000    /* [EST] N20 de 6 V */
/* Autoteste de motor e encoder do health-check: giro curto de uma roda por vez, rodas no ar. */
#define MOTOR_AUTOTESTE_POTENCIA    300     /* 30 %, a mesma da bancada dos drivers; ~500 pulsos esperados no tempo abaixo */
#define MOTOR_AUTOTESTE_MS          300
#define MOTOR_AUTOTESTE_MIN_PULSOS  50      /* PROVISÓRIO: ~9 mm de roda; ajustar com a bancada */
#define MOTOR_TRAVADO_MS            1000    /* encoder parado com PWM ativo: desliga a ponte H (stuck) */

/*
 * Posição dos ToF no robô montado [MED].
 * Origem no centro do eixo das rodas; x para a frente, y para a esquerda,
 * ângulo em graus (0 = frente, 90 = esquerda).
 *
 * O frontal fica no nariz; os laterais, em braços a 65 mm à frente do eixo, apontam para os lados (90°, não
 * os ±45° do CAD). Os três ficam a ~4 cm do chão, acima das paredes de 3,5 cm do labirinto da equipe, e os
 * braços dos laterais estão um pouco tortos (o direito para baixo, o esquerdo para cima).
 */
#define TOF_F_X_MM      87.5f
#define TOF_F_Y_MM      0.0f
#define TOF_F_ANG       0.0f
#define TOF_E_X_MM      65.0f
#define TOF_E_Y_MM      40.0f
#define TOF_E_ANG       90.0f
#define TOF_D_X_MM      65.0f
#define TOF_D_Y_MM     -40.0f
#define TOF_D_ANG      -90.0f
/*
 * Erro de cada ToF, descontado da leitura [MED] (régua contra a parede). Abaixo de TOF_MIN_CONFIAVEL_MM na
 * leitura bruta os três leem de 30 a 40 mm seja qual for a distância (3 cm vira 40, 2 cm vira 30): a leitura
 * não mede.
 *
 * Recalibrado em 25/11/2026 na bancada de calibração (tecla t, mediana de 40 leituras, régua na parede do
 * labirinto). A mesma montagem repete com ±4 mm de uma leitura para outra.
 *   Esquerdo, a 45 mm: bruta 46, 47, 49, 49, 54 e 49 -> erro de +4 mm. A 20 mm: bruta 32 a 37 (comprimida).
 *   Direito, a 45 mm: bruta 64 e 70 (parede de 3,5 cm), 61, 65 e 68 (parede alta); a 70 mm: 91 -> erro de +21 mm.
 * Até 24/11/2026 o esquerdo descontava 20 mm ("lê 2 cm a mais em todas as distâncias"): com isso o robô centrado
 * se achava ~8 mm à esquerda do meio. O braço dele é preso com fita e pode ter mudado de posição desde então.
 */
#define TOF_F_DESVIO_MM     0
#define TOF_E_DESVIO_MM     5
/*
 * Direito: na bancada (contra uma madeira) acertava, mas no labirinto, com o braço torto para baixo, leu ~20 mm a
 * mais que o esquerdo em todas as células com parede dos dois lados (24/11/2026: 61/45, 68/40, 63/45, 62/47 e
 * 70/35 mm), em qualquer rumo. Sem o desconto, o robô se achava à esquerda do meio e andava raspando a direita.
 */
#define TOF_D_DESVIO_MM     21
#define TOF_MIN_CONFIAVEL_MM 40   /* na leitura bruta, antes do desconto do erro */
/*
 * Piso do direito na leitura já descontada: a compressão é do sensor e vale para a leitura bruta, então, com os
 * 20 mm de erro, o direito mede bem até ~20 mm. Com o piso de 40 na leitura descontada (até 24/11/2026), o robô
 * centrado lia ~42 mm e bastava ficar 3 mm à direita do meio para o direito parar de medir.
 */
#define TOF_D_MIN_CONFIAVEL_MM (TOF_MIN_CONFIAVEL_MM - TOF_D_DESVIO_MM)
/*
 * O esquerdo comprime de perto como os outros (25/11/2026: a 20 mm da parede a bruta deu 32 a 37 mm), então o
 * piso dele segue a mesma regra do direito. Até 24/11/2026 era tratado como se medisse bem até 1,5 cm.
 */
#define TOF_E_MIN_CONFIAVEL_MM (TOF_MIN_CONFIAVEL_MM - TOF_E_DESVIO_MM)
/* I²C dos ToF: 50 kHz tolera os jumpers da bancada; subir na placa definitiva. */
#define TOF_I2C_HZ          50000
#define TOF_ALCANCE_MAX_MM  1200  /* acima disso o VL53L0X não mede com confiança */

/*
 * Frontal com o chassi centrado e parede à frente: 82,5 - (87,5 - 31,25) = 26 mm, abaixo da faixa confiável
 * (o sensor mostra 30 a 40). A correção pela frente só usa leituras confiáveis.
 */
#define MOV_FRENTE_ALVO_MM          (CELULA_LIVRE_MEIA_MM - (TOF_F_X_MM - EIXO_RECUO_MM))

/*
 * Detecção de parede pelos ToF (FIRM-03, #129).
 *
 * Com o chassi centrado na célula, o frontal lê ~26 mm com parede e ~206 mm sem (a parede da célula
 * seguinte).
 *
 * Os laterais apontam para os lados, a 65 mm à frente do eixo: parado, o sensor fica 34 mm à frente do
 * centro da célula e mede a parede da própria célula. Com parede e o robô centrado, leem ~42 mm; com o robô
 * deslocado depois de um giro, de ~20 a ~65 mm. Sem parede, o feixe atravessa a célula vizinha e lê mais de
 * ~220 mm (ou nada).
 *
 * Histerese: vira parede abaixo de ENTRA e só deixa de ser acima de SAI.
 */
#define PAREDE_FRENTE_ENTRA_MM      120
#define PAREDE_FRENTE_SAI_MM        150
/* Parede lateral de verdade lê até ~90 mm; entre 95 e 140 o sensor direito pegava poste (pista, 26/11/2026). */
#ifndef PAREDE_LADO_ENTRA_MM
#define PAREDE_LADO_ENTRA_MM        120
#endif
#ifndef PAREDE_LADO_SAI_MM
#define PAREDE_LADO_SAI_MM          150
#endif

/*
 * Giros com o eixo atrás do centro (docs/decisoes/024). Raios do giro no lugar, a partir do eixo: quina da
 * frente ~100 mm, quina de trás ~56 mm.
 *   - 90°: curva em arco. O robô recua até o eixo ficar GIRO_RAIO_ARCO_MM atrás do centro e faz um quarto de
 *     círculo desse raio: a quina passa a ~10 mm da parede da frente, longe dos postes, e o robô termina no
 *     meio do corredor novo. Girar no lugar deixava ~7 mm de folga e o robô ~22 mm fora do meio.
 *   - 180°: manobra. Giro curto, ré que leva o eixo para o lado, e o resto do giro, que cabe no corredor com
 *     o eixo ~22 mm fora do meio (~4 mm de folga de cada lado); depois um "S" volta o robô ao meio.
 */
#ifndef GIRO_RAIO_ARCO_MM
/*
 * 90 -> 120 em 26/11/2026: na pista, com 90, o robô enroscou a quina dianteira na parede no começo do arco. Com
 * 120 ele recua 30 mm a mais, a quina de fora passa a ~14 mm da parede da frente (eram ~10) e a roda de dentro
 * anda a 64 % da velocidade do eixo (eram 52 %), mais longe da zona morta do motor.
 */
#define GIRO_RAIO_ARCO_MM           120.0f  /* raio do arco do eixo no giro de 90° */
#endif
/*
 * Ré a mais antes do arco de 90°, para experimentar na pista (-DGIRO_RECUO_EXTRA_MM=12.0f). Na bancada de
 * calibração (26/11/2026), partindo com o nariz a 115 mm da parede da frente (o previsto é 112,5), o arco de
 * 120 mm passou duas vezes e terminou com o robô reto, mas 11 e 16 mm além do meio do corredor novo (lateral 26 e
 * 31 mm, para 42,5 no meio); partindo ~1 a 2 cm adiantado, a quina dianteira de fora enroscou na parede da frente
 * a ~48° do giro. Fica em 0 porque no simulador, que faz o arco exato, qualquer ré a mais (5, 8 ou 12 mm) tira o
 * robô do meio do corredor novo e ele encosta em algum labirinto de teste.
 */
#ifndef GIRO_RECUO_EXTRA_MM
#define GIRO_RECUO_EXTRA_MM         0.0f
#endif
/*
 * O mesmo, quando não há parede à frente (-DGIRO_RECUO_EXTRA_ABERTO_MM=-10.0f). Sem parede à frente a quina de
 * fora não tem onde pegar, e quem aperta é o poste de dentro: com +12 mm, nas duas corridas de 26/11/2026 o arco
 * travou a ~95% e o robô saiu a 22 mm da parede de dentro do corredor novo (o meio dá ~50). Cada mm de ré a mais
 * tira ~0,75 mm da folga do lado de dentro até o poste (21 mm com 0, 12 mm com +12).
 */
/* 1 = sem parede à frente, o giro de 90° é no lugar, com o eixo no centro da célula, em vez do arco. */
#ifndef GIRO_NO_CENTRO_SE_ABERTO
#define GIRO_NO_CENTRO_SE_ABERTO    0
#endif
/* Com parede à frente, a ré antes do arco vai até o sensor da frente ler isto (0 = ré fixa, pela odometria). */
#ifndef GIRO_FRENTE_ANTES_DO_ARCO_MM
#define GIRO_FRENTE_ANTES_DO_ARCO_MM 0
#endif
#define RE_ALINHAR_MIN_GRAUS         4.0f   /* antes da ré de uma célula, desvira o rumo estimado acima disto */
#define GIRO_RECUO_NOVA_TENTATIVA_MM 15.0f  /* ré a mais antes de refazer um arco que enroscou */
/* Avanço que enroscou torto: a nova tentativa sai virada este tanto além de reta, para o lado livre. */
#ifndef NOVA_TENTATIVA_DESVIO_FATOR
#define NOVA_TENTATIVA_DESVIO_FATOR 0.6f
#endif
#define NOVA_TENTATIVA_DESVIO_MAX_GRAUS 8.0f
#ifndef GIRO_CENTRO_ATRAS_MM
#define GIRO_CENTRO_ATRAS_MM         8.0f    /* no giro de 90° no lugar, quanto o eixo fica atrás do centro da célula */
#endif
#ifndef GIRO_RECUO_EXTRA_ABERTO_MM
#define GIRO_RECUO_EXTRA_ABERTO_MM  0.0f
#endif
#define GIRO_ALFA_MEIA_VOLTA_GRAUS  15.0f   /* giro curto da meia-volta e do "S": com o eixo no meio, a quina vai a ~71 mm do centro */
#define GIRO_FOLGA_FRENTE_MM        10.0f   /* folga do nariz até a parede da célula seguinte no "S" da meia-volta */

#define TOF_MEDIANA_AMOSTRAS        3     /* a 20 Hz: ~100 ms de atraso, ~25 mm a 250 mm/s */
#define TOF_FALHAS_PARA_DESCARTAR   3     /* leituras seguidas sem resposta (150 ms a 20 Hz) */
#define TOF_AUTOTESTE_AMOSTRAS      5
#define TOF_AUTOTESTE_MIN_MM        20    /* abaixo disso, sensor tampado ou com defeito */

/* Bateria — LiPo 2S 7,4 V 650 mAh [ENE] (4.2 v1.5) */
#define BATERIA_DIVISOR             3.2f    /* [HW] divisor 22 kΩ / 10 kΩ depois do diodo: V_ponte = V_adc x 3,2 */
#define BATERIA_QUEDA_DIODO_MV      350     /* [ENE] queda do SS34, somada à leitura antes dos limites; calibrar em 7.2 */
#define BATERIA_MEDIA_AMOSTRAS      16      /* média móvel contra o ruído das escovas (4.2 pede no mínimo 8) */
#define BATERIA_LIMIAR_FONTE_MV     6700    /* acima disso é a LiPo (nunca abaixo de 7,0 V em uso); abaixo, as pilhas (no máximo ~6,4 V) */
#define BATERIA_CHEIA_MV            8400    /* [ENE] 4,2 V por célula */
#define BATERIA_CORTE_MV            7000    /* [ENE] corte por subtensão: 3,5 V por célula */
#define BATERIA_ALERTA_MV           7400    /* [ENE] alerta (LED + buzzer): 3,7 V por célula, acima do corte */

/* Fonte alternativa — 4 pilhas AA em série, selecionada por jumper [ENE] */
#define BATERIA_AA_ALERTA_MV        5000    /* [ENE] alerta de carga mínima das pilhas */
#define BATERIA_AA_CORTE_MV         5000    /* [ENE] o 4.2 não separa corte das pilhas: low_battery abaixo de 5,0 V */

/*
 * Curvas de descarga: pares {mV, %}, da tensão maior para a menor; o firmware
 * interpola entre os pontos (FIRM-06, #128).
 * PROVISÓRIO: curva típica de LiPo e de pilha alcalina sob a corrente média de
 * 0,43 A [ENE]. Será trocada pela curva medida nos testes de energia (7.2).
 * Nas duas fontes, 0 % é o corte: é a carga que o robô ainda pode usar.
 */
#define BATERIA_CURVA_LIPO { \
    {8400, 100}, {8200, 90}, {8000, 75}, {7800, 55}, \
    {7600, 35},  {7400, 15}, {7200, 5},  {7000, 0} }
#define BATERIA_CURVA_AA { \
    {6400, 100}, {6000, 80}, {5600, 50}, {5200, 20}, \
    {5000, 0} }

#endif /* CONFIG_ROBO_H */
