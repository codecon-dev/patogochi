// Luzes (barra de LEDs da base M5GO Bottom2) e vibração do Core2.
// Tudo não bloqueante: chame efeitosAtualiza() no loop.
#pragma once
#include <stdint.h>

enum Luz : uint8_t {
  LUZ_APAGADA,
  LUZ_REC,        // vermelho pulsando (gravando)
  LUZ_PENSANDO,   // azul girando
  LUZ_COMENDO,    // amarelo piscando
  LUZ_FESTA,      // arco-íris girando
  LUZ_MAL,        // vermelho pulsando rápido (passando mal)
  LUZ_FOME,       // vermelho fraco pulsando devagar
};

enum Vibra : uint8_t {
  VIBRA_CURTA,    // toque / começo de gravação
  VIBRA_DUPLA,    // cancelou
  VIBRA_NHAC,     // comendo
  VIBRA_FESTA,    // empolgado
  VIBRA_ENJOO,    // passando mal
  VIBRA_MORTE,    // morreu
};

void efeitosInicia();
void efeitosAtualiza(uint32_t agora);
void luz(Luz l);
void vibra(Vibra v);
// pisca a barra inteira numa cor por `ms` e volta pra luz atual
void luzFlash(uint8_t r, uint8_t g, uint8_t b, uint16_t ms = 180);
