// Copie pra config.h (que fica fora do git) e preencha.
//   cp firmware/include/config.example.h firmware/include/config.h
#pragma once

// WiFi: o Core2 só conecta em rede 2.4 GHz
#define WIFI_SSID "nome-da-rede"
#define WIFI_PASS "senha-da-rede"

// Endereço do servidor, sem barra no final.
// Mock no Mac: http://<IP do Mac>:8080  (IP: ipconfig getifaddr en0)
#define SERVIDOR_URL "http://192.168.1.8:8080"
#define PATO_TOKEN "troque-isso"
