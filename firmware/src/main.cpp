// Patogochi — etapa 2: o pato te ouve.
// Botões de toque embaixo da tela:
//   C (direita) segurando = grava ideia (solta pra enviar, arrasta pro lado pra cancelar)
// Toque no pato = quack. Segurar no pato 10 s = painel de debug.
#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "sprites.h"
#include "sons.h"
#include "efeitos.h"
#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

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
static constexpr uint16_t COR_REC = TFT_RED;

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

// luz de cada estado
static Luz luzDoEstado(Anim a) {
  switch (a) {
    case ANIM_OUVINDO:      return LUZ_REC;
    case ANIM_PENSANDO:     return LUZ_PENSANDO;
    case ANIM_COMENDO:      return LUZ_COMENDO;
    case ANIM_EMPOLGADO:    return LUZ_FESTA;
    case ANIM_PASSANDO_MAL: return LUZ_MAL;
    case ANIM_TRISTE_FOME:  return LUZ_FOME;
    default:                return LUZ_APAGADA;
  }
}

// vibração ao entrar em cada estado
static void vibraDoEstado(Anim a) {
  switch (a) {
    case ANIM_EMPOLGADO:    vibra(VIBRA_FESTA); break;
    case ANIM_DEBOCHANDO:   vibra(VIBRA_CURTA); break;
    case ANIM_PASSANDO_MAL: vibra(VIBRA_ENJOO); break;
    case ANIM_MORTO:        vibra(VIBRA_MORTE); break;
    default: break;
  }
}

static void sinalCancelou() {
  vibra(VIBRA_DUPLA);
}

static bool tocouNoPato(int x, int y) {
  return x >= DUCK_X && x < DUCK_X + 32 * DUCK_SCALE && y >= DUCK_Y && y < DUCK_Y + 32 * DUCK_SCALE;
}

// ---------- estado da tela ----------
static Anim estado = ANIM_PARADO;
static uint16_t frame = 0;
static uint32_t ultimoFrameMs = 0;
static uint16_t frameCoco = 0;
static uint32_t ultimoCocoMs = 0;
static int cocos = 0;
static Food comida = FOOD_PIPOCA;

// antes de comer: corte pra comida em tela cheia, depois o pato comendo
static constexpr uint32_t REVELA_MS = 1500;
static constexpr int COMIDA_SCALE = 12;          // 16px -> 192px
static bool revelandoComida = false;
static uint32_t inicioRevelaMs = 0;

// sequência de uma ideia: comer uma vez -> reagir por um tempo -> parado
static constexpr uint32_t REACAO_MS = 5000;
static bool comendoIdeia = false;      // false = comendo em loop (debug pelo botão C)
static Anim reacaoPendente = ANIM_PARADO;
static uint32_t fimReacaoMs = 0;       // 0 = sem reação em andamento

static void enviaEspelho();

static void comecaRevelacao() {
  revelandoComida = true;
  inicioRevelaMs = millis();
}

static void mudaEstado(Anim novo, bool comSom = true) {
  estado = novo;
  frame = 0;
  ultimoFrameMs = millis();
  revelandoComida = false;
  if (estado == ANIM_COMENDO) comecaRevelacao();
  luz(luzDoEstado(estado));
  if (comSom) {
    quackDoEstado(estado);
    vibraDoEstado(estado);
  }
}

// ---------- gravação ----------
static constexpr uint32_t TAXA_MIC = 16000;
static constexpr size_t MAX_AMOSTRAS = TAXA_MIC * 15;      // 15 s (~480 KB)
static constexpr size_t BLOCO = 1024;                      // amostras por pedaço gravado
static constexpr size_t MIN_AMOSTRAS = TAXA_MIC / 2;       // menos de 0,5 s = ignora
// O áudio sobe como multipart/form-data: o buffer tem folga antes e depois do WAV
// pra montar o corpo inteiro sem copiar as amostras.
static const char* LIMITE = "----patogochiQuackQuack";
static constexpr size_t FOLGA = 256;
static uint8_t* buffer = nullptr;                          // FOLGA + WAV + FOLGA
static uint8_t* wav = nullptr;                             // cabeçalho (44 bytes) + amostras
static int16_t* amostras = nullptr;
static size_t gravadas = 0;
static bool gravando = false;
static String ultimoErro;  // só pro painel de debug e pro log serial

// ---------- painel de debug (segura em cima do pato por 10 s) ----------
static constexpr uint32_t SEGURA_DEBUG_MS = 10000;
static bool modoDebug = false;
static uint32_t toqueNoPatoDesdeMs = 0;
static bool debugJaAlternou = false;     // evita ligar/desligar várias vezes no mesmo toque
static float ultimaSegurou = 0, ultimaGravou = 0;
static int ultimoCodigo = 0;
static String ultimaResposta;
static uint32_t inicioGravacaoMs = 0;
static constexpr uint32_t SOLTO_MS = 300;
static uint32_t soltoDesdeMs = 0;
static constexpr int ARRASTE_CANCELA = 100;   // px pro lado = cancela (igual WhatsApp)
static int xInicioToque = -1;
static int arrasteX = 0;                      // deslocamento atual, pra animar a bolinha

static void escreveCabecalhoWav(size_t n) {
  uint32_t dados = n * 2, taxaBytes = TAXA_MIC * 2, tamanho = 36 + dados, fmt = 16;
  uint16_t pcm = 1, canais = 1, bloco = 2, bits = 16;
  uint8_t* p = wav;
  memcpy(p, "RIFF", 4); memcpy(p + 4, &tamanho, 4); memcpy(p + 8, "WAVEfmt ", 8);
  memcpy(p + 16, &fmt, 4); memcpy(p + 20, &pcm, 2); memcpy(p + 22, &canais, 2);
  memcpy(p + 24, &TAXA_MIC, 4); memcpy(p + 28, &taxaBytes, 4); memcpy(p + 32, &bloco, 2);
  memcpy(p + 34, &bits, 2); memcpy(p + 36, "data", 4); memcpy(p + 40, &dados, 4);
}

static void comecaGravacao() {
  M5.Speaker.end();  // no Core2, microfone e alto-falante não funcionam juntos
  M5.Mic.begin();
  gravadas = 0;
  gravando = true;
  inicioGravacaoMs = millis();
  soltoDesdeMs = 0;
  xInicioToque = -1;
  arrasteX = 0;
  ultimoErro = "";
  mudaEstado(ANIM_OUVINDO, false);
  vibra(VIBRA_CURTA);
}

static void continuaGravacao() {
  if (gravadas + BLOCO > MAX_AMOSTRAS) return;  // chegou no limite: espera soltar o botão
  if (M5.Mic.record(amostras + gravadas, BLOCO, TAXA_MIC)) gravadas += BLOCO;
}

// devolve true se tem áudio suficiente pra enviar
static bool terminaGravacao() {
  while (M5.Mic.isRecording()) delay(1);
  M5.Mic.end();
  M5.Speaker.begin();
  gravando = false;
  return gravadas >= MIN_AMOSTRAS;
}

// ---------- envio pro servidor (roda numa tarefa separada pra animação não travar) ----------
// API: https://patogochi.codegus.com/docs.html
struct Resposta {
  bool ok = false;
  String erro, corpo;
  int codigo = 0;
  int comida = -1;                 // índice em FOODS, -1 = nenhuma
  Anim reacao = ANIM_EMPOLGADO;
};
static Resposta resposta;
static volatile bool enviando = false;
static volatile bool respostaPronta = false;

static int indiceComida(const char* nome) {
  if (!nome) return -1;
  for (int i = 0; i < FOOD_COUNT; i++) if (strcmp(FOODS[i].name, nome) == 0) return i;
  return -1;
}

static Anim animPorNome(const char* nome, Anim padrao) {
  if (!nome) return padrao;
  for (int i = 0; i < ANIM_COUNT; i++) if (strcmp(ANIMS[i].name, nome) == 0) return (Anim)i;
  return padrao;
}

static void comecaHttp(HTTPClient& http, WiFiClientSecure& seguro, const String& caminho) {
  String url = String(SERVIDOR_URL) + caminho;
  if (url.startsWith("https://")) {
    seguro.setInsecure();  // não confere o certificado (simples; dá pra fixar o certificado depois)
    http.begin(seguro, url);
  } else {
    http.begin(url);
  }
}

// Monta o corpo multipart em volta do WAV. Devolve início e tamanho.
static uint8_t* montaMultipart(size_t& tamanho) {
  char antes[FOLGA], depois[64];
  int nAntes = snprintf(antes, sizeof(antes),
      "--%s\r\nContent-Disposition: form-data; name=\"origem\"\r\n\r\ncore2\r\n"
      "--%s\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"ideia.wav\"\r\n"
      "Content-Type: audio/wav\r\n\r\n", LIMITE, LIMITE);
  int nDepois = snprintf(depois, sizeof(depois), "\r\n--%s--\r\n", LIMITE);
  size_t nWav = 44 + gravadas * 2;
  uint8_t* inicio = wav - nAntes;
  memcpy(inicio, antes, nAntes);
  memcpy(wav + nWav, depois, nDepois);
  tamanho = nAntes + nWav + nDepois;
  return inicio;
}

static void tarefaEnvio(void*) {
  Resposta r;
  if (WiFi.status() != WL_CONNECTED) {
    r.erro = "sem wifi";
  } else {
    // ordem importa: o cliente precisa ser destruído DEPOIS do HTTPClient,
    // senão o destrutor do HTTPClient usa um cliente que já morreu (crash)
    WiFiClientSecure seguro;
    HTTPClient http;
    comecaHttp(http, seguro, "/ideia.php");
    http.addHeader("Content-Type", String("multipart/form-data; boundary=") + LIMITE);
    http.addHeader("X-Api-Token", PATO_TOKEN);
    http.setTimeout(45000);  // transcrição + Claude podem demorar
    size_t tamanho;
    uint8_t* corpoEnvio = montaMultipart(tamanho);
    int codigo = http.POST(corpoEnvio, tamanho);
    String corpo = codigo > 0 ? http.getString() : "";
    http.end();
    Serial.printf("LOG POST /ideia.php -> %d %s\n", codigo, corpo.c_str());
    r.codigo = codigo;
    r.corpo = corpo;

    JsonDocument doc;
    bool jsonOk = !deserializeJson(doc, corpo);
    if (codigo == 200 && jsonOk) {
      r.ok = true;
      r.comida = indiceComida(doc["tipo"]);
      r.reacao = animPorNome(doc["estado"], ANIM_EMPOLGADO);

    } else if (codigo == 409) {
      r.erro = "pato morto (409)";
    } else if (codigo <= 0) {
      r.erro = "servidor não respondeu";
    } else {
      r.erro = jsonOk && doc["error"].is<const char*>() ? String(doc["error"].as<const char*>())
                                                        : "Erro " + String(codigo);
    }
  }
  resposta = r;
  respostaPronta = true;
  enviando = false;
  vTaskDelete(nullptr);
}

static void enviaIdeia() {
  escreveCabecalhoWav(gravadas);
  enviando = true;
  respostaPronta = false;
  mudaEstado(ANIM_PENSANDO, false);
  xTaskCreatePinnedToCore(tarefaEnvio, "envio", 16384, nullptr, 1, nullptr, 0);
}

static void trataResposta() {
  respostaPronta = false;
  ultimoCodigo = resposta.codigo;
  ultimaResposta = resposta.corpo;
  if (!resposta.ok) {
    // nada escrito na tela: o pato só debocha (erro vai pro log e pro rótulo de debug)
    ultimoErro = resposta.erro;
    Serial.printf("LOG erro: %s\n", resposta.erro.c_str());
    mudaEstado(ANIM_DEBOCHANDO);
    fimReacaoMs = millis() + REACAO_MS;
    return;
  }
  if (resposta.comida >= 0) {
    comida = (Food)resposta.comida;
    comendoIdeia = true;
    reacaoPendente = resposta.reacao;
    mudaEstado(ANIM_COMENDO, false);
  } else {
    mudaEstado(resposta.reacao);
    fimReacaoMs = millis() + REACAO_MS;
  }
}

// ---------- desenho ----------
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

static void desenhaPainelDebug() {
  String linhas[12];
  int n = 0;
  bool wifiOk = WiFi.status() == WL_CONNECTED;
  linhas[n++] = "DEBUG  (segura no pato 10s pra fechar)";
  linhas[n++] = "estado: " + String(ANIMS[estado].name) + "   cocos: " + String(cocos);
  linhas[n++] = wifiOk ? "wifi: " + WiFi.SSID() + "  " + String(WiFi.RSSI()) + " dBm" : "wifi: DESCONECTADO";
  linhas[n++] = "ip: " + (wifiOk ? WiFi.localIP().toString() : String("-"));
  linhas[n++] = "servidor: " + String(SERVIDOR_URL);
  linhas[n++] = "ultima gravacao: segurou " + String(ultimaSegurou, 1) + "s, audio " + String(ultimaGravou, 1) + "s";
  linhas[n++] = "ultima resposta: " + (ultimoCodigo ? String(ultimoCodigo) : String("-"));
  linhas[n++] = "  " + ultimaResposta.substring(0, 48);
  linhas[n++] = "ultimo erro: " + (ultimoErro.length() ? ultimoErro : String("-"));
  linhas[n++] = "bateria: " + String(M5.Power.getBatteryLevel()) + "%" + (M5.Power.isCharging() ? " (carregando)" : "");
  linhas[n++] = "ligado ha " + String(millis() / 60000) + " min   psram livre: " + String(ESP.getFreePsram() / 1024) + " KB";

  canvas.fillRoundRect(4, 4, SCREEN_W - 8, n * 12 + 8, 6, TFT_BLACK);
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_GREEN);
  canvas.setTextDatum(top_left);
  for (int i = 0; i < n; i++) canvas.drawString(linhas[i], 10, 8 + i * 12);
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

  if (gravando) {
    // bolinha vermelha piscando + segundos gravados
    // bolinha acompanha o arraste; perto de cancelar vira um X
    int bx = SCREEN_W - 20 + arrasteX / 2;
    if (bx > SCREEN_W - 10) bx = SCREEN_W - 10;
    if (abs(arrasteX) > ARRASTE_CANCELA / 2) {
      canvas.drawLine(bx - 7, 9, bx + 7, 23, COR_REC);
      canvas.drawLine(bx - 7, 23, bx + 7, 9, COR_REC);
    } else if ((millis() / 400) % 2) {
      canvas.fillCircle(bx, 16, 7, COR_REC);
    }
    canvas.setTextColor(COR_TEXTO);
    canvas.setTextDatum(middle_right);
    canvas.setTextSize(2);
    canvas.drawString(String(gravadas / (float)TAXA_MIC, 1) + "s", SCREEN_W - 32, 17);
  }

  if (modoDebug) desenhaPainelDebug();

  canvas.pushSprite(0, 0);
  enviaEspelho();
}

static void enviaEspelho() {
  // espelho: tools/espelho/ lê essa linha pelo USB e redesenha no navegador
  Serial.printf("PATO %s %u %d %u %s %d\n", ANIMS[estado].name, frame, cocos, frameCoco,
                FOODS[comida].name, revelandoComida ? 1 : 0);
}

// ---------- setup / loop ----------
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
  efeitosInicia();

  buffer = (uint8_t*)ps_malloc(FOLGA + 44 + MAX_AMOSTRAS * 2 + FOLGA);
  wav = buffer + FOLGA;
  amostras = (int16_t*)(wav + 44);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // conecta em segundo plano

  desenhaCena();
}

void loop() {
  M5.update();
  uint32_t agora = millis();
  bool mudou = false;

  // gravação: segura C (direita, como no WhatsApp) pra falar, solta pra enviar
  if (!gravando && !enviando && M5.BtnC.wasHold()) { comecaGravacao(); mudou = true; }
  if (gravando) {
    continuaGravacao();
    // "segurando" = qualquer toque na tela, assim dá pra arrastar pra fora do B.
    // O toque falha por instantes: só conta como solto depois de SOLTO_MS sem toque.
    if (M5.Touch.getCount() > 0) {
      soltoDesdeMs = 0;
      auto t = M5.Touch.getDetail();
      if (xInicioToque < 0) xInicioToque = t.base_x;
      arrasteX = t.x - xInicioToque;
      if (abs(arrasteX) >= ARRASTE_CANCELA) {
        terminaGravacao();
        Serial.printf("LOG gravacao cancelada (arrastou %dpx)\n", arrasteX);
        sinalCancelou();
        mudaEstado(ANIM_PARADO, false);
        mudou = true;
      }
    } else if (!soltoDesdeMs) {
      soltoDesdeMs = agora;
      Serial.printf("LOG solto aos %.1fs (esperando confirmar)\n", (agora - inicioGravacaoMs) / 1000.0f);
    }
    if (gravando && soltoDesdeMs && agora - soltoDesdeMs >= SOLTO_MS) {
      soltoDesdeMs = 0;
      bool ok = terminaGravacao();
      ultimaSegurou = (agora - inicioGravacaoMs) / 1000.0f;
      ultimaGravou = gravadas / (float)TAXA_MIC;
      Serial.printf("LOG gravacao: segurou %.1fs, gravou %.1fs de audio\n",
                    (agora - inicioGravacaoMs) / 1000.0f, gravadas / (float)TAXA_MIC);
      if (ok) enviaIdeia();
      else mudaEstado(ANIM_PARADO, false);
      mudou = true;
    }
  }
  if (respostaPronta) { trataResposta(); mudou = true; }

  bool ocupado = gravando || enviando;
  if (!ocupado) {
    auto toque = M5.Touch.getDetail();

    // segurar em cima do pato por 10 s liga/desliga o painel de debug
    if (M5.Touch.getCount() > 0 && tocouNoPato(toque.base_x, toque.base_y)) {
      if (!toqueNoPatoDesdeMs) toqueNoPatoDesdeMs = agora;
      if (!debugJaAlternou && agora - toqueNoPatoDesdeMs >= SEGURA_DEBUG_MS) {
        modoDebug = !modoDebug;
        debugJaAlternou = true;
        vibra(VIBRA_DUPLA);
        Serial.printf("LOG modo debug %s\n", modoDebug ? "ligado" : "desligado");
        mudou = true;
      }
    } else {
      toqueNoPatoDesdeMs = 0;
      debugJaAlternou = false;
    }

    if (toque.wasPressed() && !revelandoComida && tocouNoPato(toque.x, toque.y)) {
      vibra(VIBRA_CURTA);
      quack(estado == ANIM_EMPOLGADO ? QUACK_FELIZ
            : estado == ANIM_PASSANDO_MAL ? QUACK_ENJOADO
            : estado == ANIM_TRISTE_FOME ? QUACK_TRISTE
            : estado == ANIM_MORTO ? QUACK_MORTE : QUACK_NORMAL);
    }
  }

  // fim da reação a uma ideia -> volta pro parado
  if (fimReacaoMs && (int32_t)(agora - fimReacaoMs) >= 0) {
    fimReacaoMs = 0;
    mudaEstado(ANIM_PARADO, false);
    mudou = true;
  }

  if (revelandoComida) {
    if (agora - inicioRevelaMs >= REVELA_MS) {
      revelandoComida = false;
      vibra(VIBRA_NHAC);
      frame = 0;
      ultimoFrameMs = agora;
      mudou = true;
    }
  } else if (agora - ultimoFrameMs >= FRAME_MS[estado]) {
    frame = (frame + 1) % ANIMS[estado].frames;
    ultimoFrameMs = agora;
    if (frame == 0 && estado == ANIM_COMENDO) {
      if (comendoIdeia) {
        // comeu a ideia: agora reage
        comendoIdeia = false;
        mudaEstado(reacaoPendente);
        fimReacaoMs = agora + REACAO_MS;
      } else {
        // debug: próxima comida em loop
        comida = (Food)((comida + 1) % FOOD_COUNT);
        comecaRevelacao();
      }
    }
    mudou = true;
  }
  if (cocos > 0 && agora - ultimoCocoMs >= FRAME_MS[ANIM_COCO]) {
    frameCoco = (frameCoco + 1) % ANIMS[ANIM_COCO].frames;
    ultimoCocoMs = agora;
    mudou = true;
  }
  if (gravando) mudou = true;  // atualiza o contador de segundos
  if (modoDebug && agora % 1000 < 15) mudou = true;  // painel atualiza ~1x por segundo

  efeitosAtualiza(agora);
  if (mudou) desenhaCena();
  delay(gravando ? 1 : 10);
}
