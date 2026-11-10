// Patogochi — etapa 1: pato vivo na tela.
// Botões de toque embaixo da tela (modo debug):
//   A = estado anterior · B = põe/tira cocô · C = próximo estado
// Toque no pato = quack.
#include <M5Unified.h>
#include "sprites.h"
#include "sons.h"

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
static constexpr uint16_t COR_FUNDO = 0x9E89;  // verde
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

// ---------- quack ----------
// Mesmo som em velocidades diferentes: mais rápido = agudo/feliz, mais lento = grave/enjoado.
// Triste é outro arquivo (assets/sons/quack_triste.wav): arrastado e sumindo.
enum Quack { QUACK_NORMAL, QUACK_FELIZ, QUACK_ENJOADO, QUACK_TRISTE, QUACK_MORTE };

static void quack(Quack tipo) {
  uint32_t taxa = SOM_SAMPLE_RATE;
  uint32_t vezes = 1;
  switch (tipo) {
    case QUACK_NORMAL: break;
    case QUACK_FELIZ:  taxa = SOM_SAMPLE_RATE * 5 / 4; vezes = 2; break;  // agudo, duas vezes
    case QUACK_ENJOADO: taxa = SOM_SAMPLE_RATE * 2 / 3; break;             // grave e arrastado
    case QUACK_MORTE:  taxa = SOM_SAMPLE_RATE / 2; break;                 // bem lento
    case QUACK_TRISTE:
      M5.Speaker.playRaw(SOM_QUACK_TRISTE, SOM_QUACK_TRISTE_LEN, SOM_SAMPLE_RATE, false, 1, -1, true);
      return;
  }
  M5.Speaker.playRaw(SOM_QUACK, SOM_QUACK_LEN, taxa, false, vezes, -1, true);
}

// som automático ao entrar em cada estado
static void quackDoEstado(Anim a) {
  switch (a) {
    case ANIM_EMPOLGADO:    quack(QUACK_FELIZ); break;
    case ANIM_DEBOCHANDO:   quack(QUACK_NORMAL); break;
    case ANIM_PASSANDO_MAL: quack(QUACK_ENJOADO); break;
    case ANIM_TRISTE_FOME:  quack(QUACK_TRISTE); break;
    case ANIM_MORTO:        quack(QUACK_MORTE); break;
    default: break;
  }
}

static bool tocouNoPato(int x, int y) {
  return x >= DUCK_X && x < DUCK_X + 32 * DUCK_SCALE && y >= DUCK_Y && y < DUCK_Y + 32 * DUCK_SCALE;
}

static Anim estado = ANIM_PARADO;
static uint16_t frame = 0;
static uint32_t ultimoFrameMs = 0;
static uint16_t frameCoco = 0;
static uint32_t ultimoCocoMs = 0;
static int cocos = 0;
static Food comida = FOOD_PIPOCA;  // troca a cada vez que ele termina de comer

// antes de comer: corte pra comida em tela cheia, depois o pato comendo
static constexpr uint32_t REVELA_MS = 1500;
static constexpr int COMIDA_SCALE = 12;          // 16px -> 192px
static bool revelandoComida = false;
static uint32_t inicioRevelaMs = 0;

static void comecaRevelacao() {
  revelandoComida = true;
  inicioRevelaMs = millis();
}

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

static void enviaEspelho() {
  // espelho: tools/espelho/ lê essa linha pelo USB e redesenha no navegador
  Serial.printf("PATO %s %u %d %u %s %d\n", ANIMS[estado].name, frame, cocos, frameCoco,
                FOODS[comida].name, revelandoComida ? 1 : 0);
}

static void desenhaCena() {
  canvas.fillScreen(COR_FUNDO);

  if (revelandoComida) {
    const Sprite& c = FOODS[comida];
    desenhaSprite(c, 0, (SCREEN_W - c.w * COMIDA_SCALE) / 2, (SCREEN_H - c.h * COMIDA_SCALE) / 2,
                  COMIDA_SCALE);
    canvas.pushSprite(0, 0);
    enviaEspelho();
    return;
  }

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
  enviaEspelho();
}

static void trocaEstado(int delta) {
  // COCO é overlay, não estado do pato: pula ele
  int n = ANIM_COCO;
  estado = (Anim)(((int)estado + delta + n) % n);
  frame = 0;
  ultimoFrameMs = millis();
  revelandoComida = false;
  if (estado == ANIM_COMENDO) comecaRevelacao();
  quackDoEstado(estado);
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("PATOGOCHI boot");
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  M5.Speaker.setVolume(160);
  desenhaCena();
}

void loop() {
  M5.update();
  uint32_t agora = millis();
  bool mudou = false;

  if (M5.BtnA.wasPressed()) { trocaEstado(-1); mudou = true; }
  if (M5.BtnC.wasPressed()) { trocaEstado(+1); mudou = true; }
  if (M5.BtnB.wasPressed()) { cocos = (cocos + 1) % (MAX_COCOS + 1); mudou = true; }

  auto toque = M5.Touch.getDetail();
  if (toque.wasPressed() && !revelandoComida && tocouNoPato(toque.x, toque.y)) {
    quack(estado == ANIM_EMPOLGADO ? QUACK_FELIZ
          : estado == ANIM_PASSANDO_MAL ? QUACK_ENJOADO
          : estado == ANIM_TRISTE_FOME ? QUACK_TRISTE
          : estado == ANIM_MORTO ? QUACK_MORTE : QUACK_NORMAL);
  }

  if (revelandoComida) {
    if (agora - inicioRevelaMs >= REVELA_MS) {
      revelandoComida = false;
      frame = 0;
      ultimoFrameMs = agora;
      mudou = true;
    }
  } else if (agora - ultimoFrameMs >= FRAME_MS[estado]) {
    frame = (frame + 1) % ANIMS[estado].frames;
    ultimoFrameMs = agora;
    // terminou de comer: próxima comida (debug, depois vem do servidor)
    if (frame == 0 && estado == ANIM_COMENDO) {
      comida = (Food)((comida + 1) % FOOD_COUNT);
      comecaRevelacao();
    }
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
