// Patogochi — etapa 1: pato vivo na tela.
// Botões de toque embaixo da tela (modo debug):
//   A = estado anterior · B = põe/tira cocô · C = próximo estado
#include <M5Unified.h>
#include "sprites.h"

// ---------- layout ----------
static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 240;
static constexpr int DUCK_SCALE = 6;                         // 32px -> 192px
static constexpr int DUCK_X = (SCREEN_W - 32 * DUCK_SCALE) / 2;
static constexpr int GROUND_Y = 214;                         // linha do chão
static constexpr int DUCK_Y = GROUND_Y - 32 * DUCK_SCALE + 6;
static constexpr int COCO_SCALE = 3;
static constexpr int MAX_COCOS = 4;

// cores (RGB565)
static constexpr uint16_t COR_CEU = 0xCF7F;    // azul claro
static constexpr uint16_t COR_CHAO = 0x9E89;   // verde grama
static constexpr uint16_t COR_TEXTO = 0x2104;

// ms por frame de cada animação (ajustar olhando no aparelho)
static const uint16_t FRAME_MS[ANIM_COUNT] = {
  /* PARADO       */ 250,
  /* OUVINDO      */ 220,
  /* PENSANDO     */ 300,
  /* COMENDO      */ 90,
  /* EMPOLGADO    */ 200,
  /* DEBOCHANDO   */ 260,
  /* PASSANDO_MAL */ 90,
  /* TRISTE_FOME  */ 90,
  /* DORMINDO     */ 600,
  /* MORTO        */ 1000,
  /* COCO         */ 150,
};

static M5Canvas canvas(&M5.Display);

static Anim estado = ANIM_PARADO;
static uint16_t frame = 0;
static uint32_t ultimoFrameMs = 0;
static uint16_t frameCoco = 0;
static uint32_t ultimoCocoMs = 0;
static int cocos = 0;

// Desenha um frame ampliado, pulando os pixels transparentes.
static void desenhaSprite(const Sprite& s, uint16_t f, int x, int y, int escala) {
  const uint16_t* px = s.data + (size_t)f * s.w * s.h;
  for (int j = 0; j < s.h; j++) {
    for (int i = 0; i < s.w; i++) {
      uint16_t c = px[j * s.w + i];
      if (c == SPRITE_TRANSPARENT) continue;
      canvas.fillRect(x + i * escala, y + j * escala, escala, escala, c);
    }
  }
}

static void desenhaCena() {
  canvas.fillScreen(COR_CEU);
  canvas.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, COR_CHAO);

  desenhaSprite(ANIMS[estado], frame, DUCK_X, DUCK_Y, DUCK_SCALE);

  // cocôs alinhados no chão, alternando lados do pato
  const Sprite& coco = ANIMS[ANIM_COCO];
  static const int cocoX[MAX_COCOS] = {12, SCREEN_W - 12 - 13 * COCO_SCALE, 60, SCREEN_W - 60 - 13 * COCO_SCALE};
  for (int i = 0; i < cocos; i++) {
    desenhaSprite(coco, (frameCoco + i * 3) % coco.frames, cocoX[i],
                  GROUND_Y - coco.h * COCO_SCALE + 4, COCO_SCALE);
  }

  // rótulo de debug
  canvas.setTextColor(COR_TEXTO);
  canvas.setTextDatum(top_left);
  canvas.setTextSize(1);
  canvas.drawString(ANIMS[estado].name, 6, 6);
  canvas.setTextDatum(bottom_center);
  canvas.drawString("< estado     coco     estado >", SCREEN_W / 2, SCREEN_H - 2);

  canvas.pushSprite(0, 0);

  // espelho: tools/espelho/ lê essa linha pelo USB e redesenha no navegador
  Serial.printf("PATO %s %u %d %u\n", ANIMS[estado].name, frame, cocos, frameCoco);
}

static void trocaEstado(int delta) {
  // COCO é overlay, não estado do pato: pula ele
  int n = ANIM_COCO;
  estado = (Anim)(((int)estado + delta + n) % n);
  frame = 0;
  ultimoFrameMs = millis();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  desenhaCena();
}

void loop() {
  M5.update();
  uint32_t agora = millis();
  bool mudou = false;

  if (M5.BtnA.wasPressed()) { trocaEstado(-1); mudou = true; }
  if (M5.BtnC.wasPressed()) { trocaEstado(+1); mudou = true; }
  if (M5.BtnB.wasPressed()) { cocos = (cocos + 1) % (MAX_COCOS + 1); mudou = true; }

  if (agora - ultimoFrameMs >= FRAME_MS[estado]) {
    frame = (frame + 1) % ANIMS[estado].frames;
    ultimoFrameMs = agora;
    mudou = true;
  }
  if (cocos > 0 && agora - ultimoCocoMs >= FRAME_MS[ANIM_COCO]) {
    frameCoco = (frameCoco + 1) % ANIMS[ANIM_COCO].frames;
    ultimoCocoMs = agora;
    mudou = true;
  }

  if (mudou) desenhaCena();
  delay(10);
}
