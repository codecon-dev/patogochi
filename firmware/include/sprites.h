// GERADO por tools/gen_sprites.py — não editar à mão
#pragma once
#include <stdint.h>

#define SPRITE_TRANSPARENT 0xF81F

struct Sprite {
  const char* name;
  uint8_t w, h;
  uint16_t frames;
  const uint16_t* data;  // frames * w * h pixels RGB565
};

enum Anim : uint8_t {
  ANIM_PARADO,
  ANIM_OUVINDO,
  ANIM_PENSANDO,
  ANIM_COMENDO,
  ANIM_EMPOLGADO,
  ANIM_DEBOCHANDO,
  ANIM_PASSANDO_MAL,
  ANIM_TRISTE_FOME,
  ANIM_DORMINDO,
  ANIM_MORTO,
  ANIM_TONTO,
  ANIM_LAPIDE,
  ANIM_COCO,
  ANIM_COUNT
};

enum Food : uint8_t {
  FOOD_PIPOCA,
  FOOD_MOEDA_CHOCOLATE,
  FOOD_MINHOCA,
  FOOD_BISCOITO_SORTE,
  FOOD_COMIDA_ESTRAGADA,
  FOOD_COUNT
};

extern const Sprite ANIMS[ANIM_COUNT];
extern const Sprite FOODS[FOOD_COUNT];
