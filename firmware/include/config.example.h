// Copie pra config.h (que fica fora do git) e preencha.
//   cp firmware/include/config.example.h firmware/include/config.h
#pragma once

// WiFi: o Core2 só conecta em rede 2.4 GHz
#define WIFI_SSID "nome-da-rede"
#define WIFI_PASS "senha-da-rede"

// Redes extras (opcional): casa, hotspot do celular...
// O Core2 conecta na mais forte que estiver por perto.
// iPhone: Ajustes > Acesso Pessoal > "Maximizar Compatibilidade" (2.4 GHz)
// #define WIFI2_SSID "casa"
// #define WIFI2_PASS "senha"
// #define WIFI3_SSID "meu-celular"
// #define WIFI3_PASS "senha"

// Endereço do back-end PHP, sem barra no final (http:// ou https://)
#define SERVIDOR_URL "https://exemplo.com"
#define PATO_TOKEN "token-do-backend"
