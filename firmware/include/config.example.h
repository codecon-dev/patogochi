// Copie pra config.h (que fica fora do git) e preencha.
//   cp firmware/include/config.example.h firmware/include/config.h
#pragma once

// WiFi: o Core2 só conecta em rede 2.4 GHz
#define WIFI_SSID "nome-da-rede"
#define WIFI_PASS "senha-da-rede"

// Endereço do back-end PHP, sem barra no final (http:// ou https://)
#define SERVIDOR_URL "https://exemplo.com"
#define PATO_TOKEN "token-do-backend"
