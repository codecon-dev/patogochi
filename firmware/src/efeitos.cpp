#include "efeitos.h"
#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>

// barra de LEDs da base M5GO Bottom2 (Core2)
static constexpr int LED_PINO = 25;
static constexpr int LED_QTD = 10;
static constexpr uint8_t LED_BRILHO = 60;   // 0-255; mais que isso cansa a vista e a bateria
static constexpr uint8_t VIBRA_FORCA = 180;

static Adafruit_NeoPixel barra(LED_QTD, LED_PINO, NEO_GRB + NEO_KHZ800);
static Luz luzAtual = LUZ_APAGADA;
static uint32_t ultimoLedMs = 0;

// padrões de vibração: liga, desliga, liga, ... (ms), terminando em 0
static const uint16_t PADROES[][8] = {
  /* CURTA */ {50, 0},
  /* DUPLA */ {80, 90, 80, 0},
  /* NHAC  */ {40, 110, 40, 110, 40, 0},
  /* FESTA */ {90, 70, 90, 70, 160, 0},
  /* ENJOO */ {450, 0},
  /* MORTE */ {900, 0},
};
static const uint16_t* padrao = nullptr;
static int passo = 0;
static uint32_t proximoPassoMs = 0;

void efeitosInicia() {
  barra.begin();
  barra.setBrightness(LED_BRILHO);
  barra.clear();
  barra.show();
  M5.Power.setVibration(0);
}

void luz(Luz l) {
  luzAtual = l;
  ultimoLedMs = 0;  // redesenha já no próximo atualiza
}

void vibra(Vibra v) {
  padrao = PADROES[v];
  passo = 0;
  proximoPassoMs = 0;
}

// 0..255..0 em `periodo` ms
static uint8_t pulso(uint32_t agora, uint32_t periodo) {
  uint32_t t = agora % periodo;
  uint32_t meio = periodo / 2;
  return t < meio ? t * 255 / meio : (periodo - t) * 255 / meio;
}

static void desenhaLuz(uint32_t agora) {
  switch (luzAtual) {
    case LUZ_APAGADA:
      barra.clear();
      break;
    case LUZ_REC:
      barra.fill(barra.Color(pulso(agora, 1000), 0, 0));
      break;
    case LUZ_PENSANDO: {
      barra.clear();
      int p = (agora / 90) % LED_QTD;
      barra.setPixelColor(p, barra.Color(0, 80, 255));
      barra.setPixelColor((p + LED_QTD - 1) % LED_QTD, barra.Color(0, 20, 70));
      break;
    }
    case LUZ_COMENDO:
      barra.fill((agora / 150) % 2 ? barra.Color(255, 180, 0) : 0);
      break;
    case LUZ_FESTA:
      for (int i = 0; i < LED_QTD; i++) {
        uint16_t matiz = (agora * 40 + i * 65536 / LED_QTD) & 0xFFFF;
        barra.setPixelColor(i, barra.gamma32(barra.ColorHSV(matiz)));
      }
      break;
    case LUZ_MAL:
      barra.fill(barra.Color(pulso(agora, 500), 0, 0));
      break;
    case LUZ_FOME:
      barra.fill(barra.Color(pulso(agora, 2400) / 4, 0, 0));
      break;
  }
  barra.show();
}

void efeitosAtualiza(uint32_t agora) {
  if (agora - ultimoLedMs >= 30) {   // ~30 fps nos LEDs
    ultimoLedMs = agora;
    desenhaLuz(agora);
  }

  if (padrao && (int32_t)(agora - proximoPassoMs) >= 0) {
    uint16_t dur = padrao[passo];
    if (dur == 0) {
      M5.Power.setVibration(0);
      padrao = nullptr;
      return;
    }
    M5.Power.setVibration(passo % 2 == 0 ? VIBRA_FORCA : 0);
    proximoPassoMs = agora + dur;
    passo++;
  }
}
