#ifndef CONFIG_WIFI_LOCAL_H
#define CONFIG_WIFI_LOCAL_H

/*
 * Rede Wi-Fi do ambiente esp32_real_wifi (só para testes).
 *
 * Copie este arquivo para wifi_local.h, na mesma pasta, e preencha. O wifi_local.h não vai para o Git
 * (.gitignore), então a senha fica só no seu computador.
 *
 * A ESP32 só entra em redes de 2,4 GHz. Use a rede principal (redes de visitantes costumam isolar os
 * aparelhos, e o notebook não enxerga o robô).
 */
#define WIFI_SSID  "nome-da-rede-2.4GHz"
#define WIFI_SENHA "senha-da-rede"

#endif /* CONFIG_WIFI_LOCAL_H */
