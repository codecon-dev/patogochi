# Patogochi 🦆

Tamagotchi de pato no M5Stack Core2 que se alimenta das ideias do Gabriel.
Se ele parar de ter ideias, o pato morre de fome.

Vídeo: "Criei um Tamagotchi que morre se eu parar de ter ideias" (É o Vibecodas).

---

## Arquitetura

```
┌──────────────── Core2 ────────────────┐        ┌────────────── Servidor ──────────────┐
│ tela 320x240 + sprites 32x32 (x6)     │  WiFi  │ notebook → depois VPS       │
│ botão segura → grava áudio (mic PDM)  │ ─────► │ POST /ideia (WAV)                     │
│ máquina de estados do pato            │        │   1. transcreve (Whisper)             │
│ barra de LED (vermelho = mal)         │ ◄───── │   2. Claude classifica + opina        │
│ GET /estado a cada X min              │  JSON  │   3. salva ideia/lembrete (SQLite)    │
└───────────────────────────────────────┘        │ estado do pato (fome, cocô, vivo)     │
                                                 └───────────────────────────────────────┘
```

**Regra principal:** o estado do pato (fome, cocôs, morto) mora no **servidor**.
O Core2 só desenha e grava. Assim ele sobrevive a reboot e bateria acabando.

---

## Estados do pato (sprites em `assets/sprites/`)

| Pasta | Frames | Quando aparece |
|---|---|---|
| `parado` | 6 | Padrão, pisca |
| `ouvindo` | 3 | Enquanto o botão tá apertado gravando |
| `pensando` | 3 | Esperando o servidor responder |
| `comendo` | 35 | Ideia aceita, comida entra |
| `empolgado` | 2 | Ideia boa |
| `debochando` | 3 | Ideia meh / zoeira |
| `passando_mal` | 46 | Comeu comida estragada (ideia ruim) |
| `triste_fome` | 61 | Fome baixa |
| `dormindo` | 2 | Madrugada / sem interação |
| `morto` | 1 | Fome zerada por tempo demais |
| `coco` | 8 | Overlay: 1 cocô por lembrete vencido |

## Comidas (`assets/sprites/comidas/`) — definido pela API

| Comida | Tipo de ideia |
|---|---|
| `pipoca` | Ideia de conteúdo / vídeo |
| `moeda_chocolate` | Ideia de negócio / dinheiro |
| `biscoito_sorte` | Ideia com prazo (vira lembrete) |
| `minhoca` | Ideia sem pé nem cabeça |
| `comida_estragada` | Ideia ruim → `passando_mal` |

---|---|
| `pipoca` | Ideia de conteúdo / vídeo |
| `moeda_chocolate` | Ideia de negócio / dinheiro |
| `minhoca` | Ideia técnica / código |
| `biscoito_sorte` | Ideia de vida / futuro / sonho |
| `comida_estragada` | Ideia ruim (qualquer tipo) → `passando_mal` |

---

## Regras do jogo (números iniciais, ajustar no teste)

- Fome vai de 100 a 0. Perde **~4 por hora** (≈ 25h de cheio a vazio).
- Cada ideia boa: **+30**. Ideia ruim: **+5** e fica `passando_mal` por 10 min.
- Fome < 30 → `triste_fome` + LED vermelho.
- Fome em 0 por **12h** → `morto`. Revive com prenda (ver abaixo).
- Ideia com prazo ("me lembra de X até sexta") vira **lembrete**.
- Lembrete vencido → 1 cocô na tela. Só sai quando marcar como feito (toque no cocô).
- Cocô acumulado também tira fome mais rápido (opcional).

---

## Morte e ressurreição

1. **Pedir desculpas implorando:** segura o botão e grava. O Claude julga se foi sincero o bastante. Se não foi, o pato debocha e você tenta de novo.
2. **Massagem cardíaca:** 30 chacoalhadas no ritmo de 100 a 120 por minuto, lidas pelo acelerômetro (IMU) do Core2. A tela mostra um coração pulsando e o contador. Errou o ritmo, volta pro zero.
3. Pato revive com fome 50.
4. **Lápide:** contador de mortes 💀 fica na tela pra sempre.

## Alexa (back-end PHP)

| Função | Como | Esforço |
|---|---|---|
| **Dedo-duro** | Fome baixa, passando mal ou morte → Alexa anuncia pra casa toda. PHP chama a URL do Voice Monkey. | ~1-2h |
| **Funeral** | Pato morre → Alexa toca música fúnebre e lê o obituário escrito pelo Claude (cita a última ideia comida). Mesmo Voice Monkey. | ~1h depois do dedo-duro |
| **Alimentar pela Alexa** | "Alexa, fala pro pato: …" → skill custom (modo dev, sem publicar) → manda o texto pro mesmo endpoint de ideia do PHP. | ~1-2 dias |

Obs: o endpoint de ideia precisa aceitar **áudio (Core2) ou texto (Alexa)**.

## Etapas = passos do vídeo

| # | Entrega | Pronto quando |
|---|---|---|
| 0 | Setup | Core2 reconhecido no Mac, "hello world" na tela |
| 1 | **Pato vivo na tela** | `parado` animando, troca de estado por botão (debug) |
| 2 | **Pato me ouve** | Segura botão → grava → servidor devolve texto transcrito na tela |
| 3 | **Comida + opinião** | Claude escolhe a comida e escreve 1 frase de opinião; pato come e reage |
| 4 | **Lembrete vira cocô** | Ideia com prazo vira lembrete; vencido → cocô; toque limpa |
| 5 | **Deploy VPS** | Servidor na VPS, Core2 funciona pelo hotspot do celular |
| 6 | **Semana real** | Fome caindo de verdade, 7 dias rodando |

---

## Divisão sugerida (2 pessoas)

- **Gabriel — firmware (Core2):** sprites, animação, estados, gravação, WiFi, LED.
- **Parceiro — back-end em PHP:** endpoints, Whisper, prompt do Claude, banco, regras de fome, deploy.
- Contrato entre os dois: `POST /ideia` e `GET /estado` (definir JSON no passo 2).

---

## Decisões em aberto

- [ ] Firmware: **PlatformIO + M5Unified (C++)** (recomendado) ou UIFlow/MicroPython
- [x] Servidor: **PHP** (parceiro cuida)
- [ ] Transcrição: Whisper API (simples) ou whisper.cpp local (sem custo)
- [x] Botão de gravar: **C (direita)**, segura pra falar, solta pra enviar, arrasta pro lado cancela
- [x] Mapeamento de comidas (definido pela API)
- [x] Pato morto **revive** com prenda (ver "Morte e ressurreição")
- [x] Opinião do pato: nada escrito na tela, só a reação

## Riscos conhecidos (do roteiro)

- Driver USB-serial no Mac: testar antes de gravar.
- WiFi do Core2 é **só 2.4GHz** → iPhone em "Maximizar Compatibilidade".
- Microfone do Core2 é fraco: gravar em canto silencioso.
- Tela reflete na câmera: usar PNGs dos sprites na edição.
