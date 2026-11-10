// Patogochi — etapa 2: o pato te ouve.
// Botões de toque embaixo da tela:
// Barra de ícones embaixo, um por botão:
//   A = tarefas · B = status · C segurando = grava ideia (solta envia, arrasta pro lado cancela)
// Na tela de tarefas: A = voltar · B = próxima · C = marca feita. No status: A = voltar.
// Toque no pato = quack. Segurar no pato 10 s = painel de debug.
#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "sprites.h"
#include "sons.h"
#include "efeitos.h"
#include "icones.h"
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
static constexpr int GROUND_Y = 180;                         // linha do chão (embaixo fica a barra de ícones)
static constexpr int DUCK_Y = GROUND_Y - 32 * DUCK_SCALE + 6;
static constexpr int COCO_SCALE = 3;
static constexpr int MAX_COCOS = 4;
static constexpr int BARRA_Y = 204;                          // barra de ícones, em cima dos botões A/B/C
static constexpr int ICONE_ESCALA = 2;                       // 12px -> 24px
static const int BOTAO_X[3] = {53, 160, 267};                // centro dos botões A, B, C

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

static bool consultandoEstado();
static void tarefaEnvio(void*) {
  Resposta r;
  while (consultandoEstado()) vTaskDelay(pdMS_TO_TICKS(50));
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

// ---------- estado do servidor (o pato mora lá) ----------
static volatile bool consultando = false;
static bool consultandoEstado() { return consultando; }
// GET /estado.php a cada 2 min. O sprite que o servidor manda (parado, triste_fome, morto...)
// vira o "estado base": é pra onde o pato volta quando não está gravando/comendo/reagindo.
static constexpr uint32_t CONSULTA_MS = 2 * 60 * 1000;
static Anim estadoBase = ANIM_PARADO;
static int hp = -1, mortes = 0, versaoServidor = -1;
static uint32_t ultimaConsultaMs = 0;
static bool jaConsultou = false;
static volatile bool estadoPronto = false;
static Anim novoBase = ANIM_PARADO;
static int novoHp = -1, novasMortes = 0, novaVersao = -1;

static void tarefaEstado(void*) {
  WiFiClientSecure seguro;  // antes do HTTPClient (ver tarefaEnvio)
  HTTPClient http;
  comecaHttp(http, seguro, "/estado.php");
  http.setTimeout(10000);
  int codigo = http.GET();
  String corpo = codigo == 200 ? http.getString() : "";
  http.end();
  Serial.printf("LOG GET /estado.php -> %d %s\n", codigo, corpo.c_str());
  JsonDocument doc;
  if (codigo == 200 && !deserializeJson(doc, corpo)) {
    novoBase = animPorNome(doc["sprite"], ANIM_PARADO);
    novoHp = doc["hp"] | -1;
    novasMortes = doc["mortes"] | 0;
    novaVersao = doc["versao"] | -1;
    estadoPronto = true;
  }
  consultando = false;
  vTaskDelete(nullptr);
}

static bool redeOcupadaTelas();

static void consultaEstado() {
  if (consultando || enviando || redeOcupadaTelas() || WiFi.status() != WL_CONNECTED) return;
  consultando = true;
  ultimaConsultaMs = millis();
  jaConsultou = true;
  xTaskCreatePinnedToCore(tarefaEstado, "estado", 12288, nullptr, 1, nullptr, 0);
}

// pato sem nada pra fazer: pode mostrar o estado base
static bool ocioso() {
  return !gravando && !enviando && !comendoIdeia && !fimReacaoMs && !revelandoComida &&
         estado != ANIM_COMENDO;
}

static void voltaProBase(bool comSom) {
  mudaEstado(estadoBase, comSom);
}

// ---------- telas ----------
enum Tela { TELA_PATO, TELA_TAREFAS, TELA_STATUS };
static Tela tela = TELA_PATO;
static constexpr uint32_t TELA_VOLTA_MS = 30000;  // sem mexer por 30 s, volta pro pato
static uint32_t ultimoBotaoMs = 0;

// lembretes (GET /lembretes.php) — endpoint pedido pro back-end
static constexpr int MAX_LEMBRETES = 20;
struct Lembrete { int id; String texto; String prazo; bool vencido; bool feito; };
static Lembrete lembretes[MAX_LEMBRETES];
static int qtdLembretes = 0;
static int cursorLembrete = 0;
enum CargaLembretes { CARGA_NADA, CARGA_CARREGANDO, CARGA_OK, CARGA_ERRO };
static volatile CargaLembretes cargaLembretes = CARGA_NADA;
static String erroLembretes;
static volatile int marcandoId = 0;       // id sendo marcado como feito (0 = nenhum)
static volatile int marcadoOkId = 0;      // id que o servidor confirmou

static bool redeOcupadaTelas() { return cargaLembretes == CARGA_CARREGANDO || marcandoId; }

// "2026-11-15T18:00:00-03:00" -> "15/11"
static String diaMes(const char* iso) {
  String s = iso ? iso : "";
  return s.length() >= 10 ? s.substring(8, 10) + "/" + s.substring(5, 7) : "";
}

static void tarefaLembretes(void*) {
  while (consultando || enviando) vTaskDelay(pdMS_TO_TICKS(50));  // uma conexão por vez
  WiFiClientSecure seguro;  // antes do HTTPClient (ver tarefaEnvio)
  HTTPClient http;
  comecaHttp(http, seguro, "/lembretes.php");
  http.addHeader("X-Api-Token", PATO_TOKEN);
  http.setTimeout(10000);
  int codigo = http.GET();
  String corpo = codigo > 0 ? http.getString() : "";
  http.end();
  Serial.printf("LOG GET /lembretes.php -> %d %s\n", codigo, corpo.c_str());
  JsonDocument doc;
  if (codigo == 200 && !deserializeJson(doc, corpo)) {
    int n = 0;
    for (JsonObject l : doc["lembretes"].as<JsonArray>()) {
      if (n >= MAX_LEMBRETES) break;
      lembretes[n].id = l["id"] | 0;
      lembretes[n].texto = l["texto"] | "";
      lembretes[n].prazo = diaMes(l["vence_em"]);
      lembretes[n].vencido = l["vencido"] | false;
      lembretes[n].feito = false;
      n++;
    }
    qtdLembretes = n;
    cargaLembretes = CARGA_OK;
  } else {
    erroLembretes = codigo <= 0 ? String("servidor não respondeu") : "erro " + String(codigo);
    cargaLembretes = CARGA_ERRO;
  }
  vTaskDelete(nullptr);
}

static void carregaLembretes() {
  if (WiFi.status() != WL_CONNECTED) {
    erroLembretes = "sem wifi";
    cargaLembretes = CARGA_ERRO;
    return;
  }
  cargaLembretes = CARGA_CARREGANDO;
  qtdLembretes = 0;
  cursorLembrete = 0;
  xTaskCreatePinnedToCore(tarefaLembretes, "lembretes", 12288, nullptr, 1, nullptr, 0);
}

static void tarefaMarcaFeito(void*) {
  while (consultando || enviando) vTaskDelay(pdMS_TO_TICKS(50));  // uma conexão por vez
  int id = marcandoId;
  WiFiClientSecure seguro;
  HTTPClient http;
  comecaHttp(http, seguro, "/marcar-feito.php");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Token", PATO_TOKEN);
  http.setTimeout(10000);
  int codigo = http.POST(String("{\"lembrete_id\":") + id + "}");
  String corpo = codigo > 0 ? http.getString() : "";
  http.end();
  Serial.printf("LOG POST /marcar-feito.php %d -> %d %s\n", id, codigo, corpo.c_str());
  if (codigo == 200) marcadoOkId = id;
  marcandoId = 0;
  vTaskDelete(nullptr);
}

static void marcaFeito(int i) {
  if (i < 0 || i >= qtdLembretes || lembretes[i].feito || marcandoId) return;
  marcandoId = lembretes[i].id;
  xTaskCreatePinnedToCore(tarefaMarcaFeito, "feito", 12288, nullptr, 1, nullptr, 0);
}

static void abreTela(Tela t) {
  tela = t;
  ultimoBotaoMs = millis();
  if (t == TELA_TAREFAS) carregaLembretes();
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

// contorno = só a borda do desenho (ex.: coração vazio no status)
static void desenhaIcone(const char* const* icone, int cx, int cy, int escala, bool contorno = false) {
  int x0 = cx - ICONE_TAM * escala / 2, y0 = cy - ICONE_TAM * escala / 2;
  auto cheio = [&](int i, int j) {
    return i >= 0 && j >= 0 && i < ICONE_TAM && j < ICONE_TAM && icone[j][i] == 'k';
  };
  for (int j = 0; j < ICONE_TAM; j++)
    for (int i = 0; i < ICONE_TAM; i++) {
      if (!cheio(i, j)) continue;
      if (contorno && cheio(i - 1, j) && cheio(i + 1, j) && cheio(i, j - 1) && cheio(i, j + 1)) continue;
      canvas.fillRect(x0 + i * escala, y0 + j * escala, escala, escala, TFT_BLACK);
    }
}

// setas e check desenhados com primitivas (barra das telas internas)
static void desenhaVoltar(int cx, int cy) {
  desenhaIcone(ICONE_VOLTAR, cx, cy, ICONE_ESCALA);
}
static void desenhaSetaBaixo(int cx, int cy) {
  canvas.fillTriangle(cx, cy + 10, cx - 8, cy + 2, cx + 8, cy + 2, TFT_BLACK);
  canvas.fillRect(cx - 3, cy - 10, 6, 13, TFT_BLACK);
}
static void desenhaCheck(int cx, int cy, uint16_t cor) {
  for (int d = -2; d <= 2; d++) {
    canvas.drawLine(cx - 11, cy + d, cx - 4, cy + 7 + d, cor);
    canvas.drawLine(cx - 4, cy + 7 + d, cx + 11, cy - 8 + d, cor);
  }
}

static void desenhaBarra() {
  int cy = BARRA_Y + 18;  // centro da barra (y ~222)
  if (tela == TELA_PATO) {
    desenhaIcone(ICONE_TAREFAS, BOTAO_X[0], cy, ICONE_ESCALA);
    desenhaIcone(ICONE_CORACAO, BOTAO_X[1], cy, ICONE_ESCALA);
    desenhaIcone(ICONE_MICROFONE, BOTAO_X[2], cy, ICONE_ESCALA);
  } else {
    desenhaVoltar(BOTAO_X[0], cy);
    if (tela == TELA_TAREFAS && qtdLembretes > 0) {
      desenhaSetaBaixo(BOTAO_X[1], cy);
      desenhaCheck(BOTAO_X[2], cy, TFT_BLACK);
    }
  }
}

static void desenhaTarefas() {
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextSize(1);
  canvas.setTextColor(COR_TEXTO);
  canvas.setTextDatum(middle_left);
  if (cargaLembretes == CARGA_CARREGANDO) {
    canvas.setTextDatum(middle_center);
    canvas.drawString("carregando...", SCREEN_W / 2, 100);
  } else if (cargaLembretes == CARGA_ERRO) {
    canvas.setTextDatum(middle_center);
    canvas.drawString(erroLembretes, SCREEN_W / 2, 100);
  } else if (qtdLembretes == 0) {
    canvas.setTextDatum(middle_center);
    canvas.drawString("nenhuma tarefa \\o/", SCREEN_W / 2, 100);
  } else {
    // janela de até 6 linhas acompanhando o cursor
    constexpr int VISIVEIS = 6, ALT = 32, Y0 = 8;
    int primeiro = cursorLembrete - VISIVEIS + 1;
    if (primeiro < 0) primeiro = 0;
    for (int k = 0; k < VISIVEIS && primeiro + k < qtdLembretes; k++) {
      int i = primeiro + k;
      int y = Y0 + k * ALT;
      const Lembrete& l = lembretes[i];
      if (i == cursorLembrete) canvas.fillRoundRect(4, y, SCREEN_W - 8, ALT - 4, 6, TFT_WHITE);
      // checkbox
      int bx = 12, by = y + 4;
      canvas.drawRect(bx, by, 20, 20, COR_TEXTO);
      canvas.drawRect(bx + 1, by + 1, 18, 18, COR_TEXTO);
      bool marcando = marcandoId == l.id;
      if (l.feito) desenhaCheck(bx + 10, by + 9, 0x05E0);
      else if (marcando && (millis() / 250) % 2) canvas.fillRect(bx + 5, by + 5, 10, 10, 0xAD55);
      // texto (cortado pra caber) + prazo
      String prazo = l.prazo;
      int largPrazo = prazo.length() ? canvas.textWidth(prazo) + 8 : 0;
      String t = l.texto;
      while (t.length() > 1 && canvas.textWidth(t) > SCREEN_W - 50 - largPrazo) t.remove(t.length() - 1);
      if (t.length() < l.texto.length()) t += "…";
      canvas.setTextColor(l.feito ? 0x7BEF : COR_TEXTO);
      canvas.drawString(t, 40, y + 14);
      if (prazo.length()) {
        canvas.setTextDatum(middle_right);
        canvas.setTextColor(l.vencido && !l.feito ? 0xC800 : COR_TEXTO);  // vencido = vermelho
        canvas.drawString(prazo, SCREEN_W - 12, y + 14);
        canvas.setTextDatum(middle_left);
      }
      if (l.feito) canvas.drawFastHLine(40, y + 14, canvas.textWidth(t), 0x7BEF);
    }
  }
  canvas.setFont(&fonts::Font0);
}

static void desenhaStatus() {
  // fome em 5 corações
  int cheios = hp < 0 ? 0 : (hp + 19) / 20;
  for (int i = 0; i < 5; i++) desenhaIcone(ICONE_CORACAO, 48 + i * 56, 52, 3, i >= cheios);  // vazio = contorno
  // lápide com o número de mortes
  int lx = SCREEN_W / 2, ly = 140;
  canvas.fillRoundRect(lx - 30, ly - 34, 60, 70, 24, 0x9CD3);
  canvas.fillRect(lx - 30, ly + 10, 60, 26, 0x9CD3);
  canvas.drawRoundRect(lx - 30, ly - 34, 60, 70, 24, COR_TEXTO);
  canvas.fillRect(lx - 40, ly + 34, 80, 6, 0x6A60);  // terra
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COR_TEXTO);
  canvas.setTextDatum(middle_center);
  canvas.drawString("RIP", lx, ly - 14);
  canvas.setFont(&fonts::Font4);
  canvas.drawString(String(mortes), lx, ly + 12);
  canvas.setFont(&fonts::Font0);
}

static void desenhaPainelDebug() {
  String linhas[14];
  int n = 0;
  bool wifiOk = WiFi.status() == WL_CONNECTED;
  linhas[n++] = "DEBUG  (segura no pato 10s pra fechar)";
  linhas[n++] = "estado: " + String(ANIMS[estado].name) + "  base: " + String(ANIMS[estadoBase].name);
  linhas[n++] = "fome(hp): " + (hp >= 0 ? String(hp) : String("?")) + "  mortes: " + String(mortes) +
                "  versao: " + String(versaoServidor) + "  consulta ha " +
                (jaConsultou ? String((millis() - ultimaConsultaMs) / 1000) + "s" : String("-"));
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

  if (tela == TELA_TAREFAS) {
    desenhaTarefas();
  } else if (tela == TELA_STATUS) {
    desenhaStatus();
  } else {
    desenhaSprite(ANIMS[estado], frame, DUCK_X, DUCK_Y, DUCK_SCALE);
  }
  if (!gravando) desenhaBarra();

  // cocôs alinhados no chão, alternando lados do pato
  const Sprite& coco = ANIMS[ANIM_COCO];
  static const int cocoX[MAX_COCOS] = {12, SCREEN_W - 12 - 13 * COCO_SCALE, 60, SCREEN_W - 60 - 13 * COCO_SCALE};
  for (int i = 0; tela == TELA_PATO && i < cocos; i++) {
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
  if (tela == TELA_PATO && !gravando && !enviando && M5.BtnC.wasHold()) { comecaGravacao(); mudou = true; }
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
        voltaProBase(false);
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
      else voltaProBase(false);
      mudou = true;
    }
  }
  if (respostaPronta) { trataResposta(); mudou = true; }

  // consulta o servidor assim que o WiFi conecta e depois a cada CONSULTA_MS
  if (!gravando && WiFi.status() == WL_CONNECTED &&
      (!jaConsultou || agora - ultimaConsultaMs >= CONSULTA_MS)) {
    consultaEstado();
  }
  if (estadoPronto) {
    estadoPronto = false;
    hp = novoHp;
    mortes = novasMortes;
    versaoServidor = novaVersao;
    bool mudouBase = novoBase != estadoBase;
    estadoBase = novoBase;
    if (mudouBase && ocioso()) {
      voltaProBase(true);  // com som: ex. entrou em triste_fome ou morreu
      mudou = true;
    }
  }

  // resultados de rede das telas
  if (marcadoOkId) {
    for (int i = 0; i < qtdLembretes; i++) if (lembretes[i].id == marcadoOkId) lembretes[i].feito = true;
    marcadoOkId = 0;
    vibra(VIBRA_CURTA);
    quack(QUACK_FELIZ);
    consultaEstado();  // cocô some no servidor
    mudou = true;
  }
  static CargaLembretes cargaAnterior = CARGA_NADA;
  if (cargaLembretes != cargaAnterior) { cargaAnterior = cargaLembretes; mudou = true; }
  if (marcandoId) mudou = true;  // checkbox piscando

  // botões das telas internas
  // telaNoInicio: o mesmo aperto do A que volta pro pato não pode reabrir as tarefas
  Tela telaNoInicio = tela;
  if (tela != TELA_PATO) {
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) ultimoBotaoMs = agora;
    if (M5.BtnA.wasPressed() || agora - ultimoBotaoMs >= TELA_VOLTA_MS) {
      tela = TELA_PATO;
      mudou = true;
    } else if (tela == TELA_TAREFAS && qtdLembretes > 0) {
      if (M5.BtnB.wasPressed()) { cursorLembrete = (cursorLembrete + 1) % qtdLembretes; mudou = true; }
      if (M5.BtnC.wasPressed()) { marcaFeito(cursorLembrete); mudou = true; }
    }
  }

  bool ocupado = gravando || enviando;
  if (!ocupado && telaNoInicio == TELA_PATO) {
    if (M5.BtnA.wasPressed()) { abreTela(TELA_TAREFAS); mudou = true; }
    if (M5.BtnB.wasPressed()) { abreTela(TELA_STATUS); mudou = true; }

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
    voltaProBase(false);
    consultaEstado();  // a ideia mudou a fome: atualiza já
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
