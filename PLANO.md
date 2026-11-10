# Patogochi 🦆

Tamagotchi de pato no M5Stack Core2 que se alimenta das ideias do Gabriel.
Se ele parar de ter ideias, o pato morre de fome.

Vídeo: "Criei um Tamagotchi que morre se eu parar de ter ideias" (É o Vibecodas).

---

## Arquitetura

```
┌──────────────── Core2 (firmware) ─────────────┐         ┌──────── Back-end PHP (VPS) ────────┐
│ desenha o pato, menu, tarefas, status         │  WiFi   │ patogochi.codegus.com              │
│ C segurando → grava áudio (WAV 16 kHz)        │ ──────► │ POST /ideia.php   (whisper+Claude) │
│ chacoalhar → tonto / massagem cardíaca (IMU)  │         │ POST /reviver.php (desculpa/massag)│
│ quack, LEDs da base, vibração                 │ ◄────── │ GET  /estado.php  (a cada 10 s)    │
│ dorme 21h–7h, tela fraca parada               │  JSON   │ GET  /lembretes.php, /marcar-feito │
└───────────────────────────────────────────────┘         │ cron: fome cai, lembrete vira cocô │
                                                          └────────────────────────────────────┘
```

**Regra principal:** o estado do pato (fome, cocôs, morto, mortes) mora no **servidor**.
O Core2 só desenha, grava e reage. Sobrevive a reboot e bateria acabando.

- API oficial: https://patogochi.codegus.com/docs.html — resumo do que o firmware usa em [API.md](API.md)
- Como compilar, botões, WiFi: [README.md](README.md)

---

## Como se usa

Barra de ícones embaixo, um por botão:

| Tela | A (esquerda) | B (meio) | C (direita) |
|---|---|---|---|
| **Pato** | ☑ tarefas | ❤ status | 🎤 segura pra falar a ideia (solta envia, arrasta pro lado cancela) |
| **Tarefas** | `<` voltar | | |
| **Status** | `<` voltar | | |

- **Tarefas:** lista com checkbox, texto inteiro, data à direita (vermelha = vencida). Tocar marca como feita; arrastar rola; puxar pra baixo no topo atualiza.
- **Status:** 5 corações = fome (cada um = 20 de hp) + lápide com o número de mortes.
- **Toque no pato:** quack. **Chacoalhar:** pato tonto.
- **Segurar no pato 10 s:** painel de debug (WiFi, IP, servidor, fome, hora, última gravação/resposta/erro, bateria, memória).
- Nenhum texto de opinião na tela: o pato só reage.

---

## Estados do pato (sprites em `assets/sprites/`)

| Pasta | Frames | Quando aparece | Som | Luzes | Vibração |
|---|---|---|---|---|---|
| `parado` | 6 | Padrão | — | apagadas | — |
| `ouvindo` | 3 | Gravando ideia | — | vermelho pulsando | curta no início |
| `pensando` | 3 | Esperando o servidor | — | azul girando | — |
| `comendo` | 35 | Depois do corte pra comida em tela cheia | — | amarelo piscando | "nhac" |
| `empolgado` | 2 | Ideia boa / reviveu | quack feliz (agudo, 2x) | arco-íris | festa |
| `debochando` | 3 | Ideia meh / minhoca / erro | quack normal | apagadas | curta |
| `passando_mal` | 46 | Comida estragada (10 min) | quack enjoado (lento) | vermelho rápido | longa |
| `triste_fome` | 61 | Fome baixa (vem do servidor) | quack choroso | vermelho fraco | — |
| `dormindo` | 2 | 21h–7h, quando estaria parado | — | apagadas | — |
| `morto` | 1 | Vem do servidor | quack de morte | apagadas | bem longa |
| `tonto` | 6 | Chacoalhou o pato vivo (4 s) | quack enjoado | azul girando | longa |
| `lapide` | 1 | Tela de status, com o nº de mortes | | | |
| `coco` | 8 | 1 por lembrete vencido (até 4) | | | |

## Comidas (`assets/sprites/comidas/`) — definido pela API

| Comida | Tipo de ideia | Reação |
|---|---|---|
| `pipoca` | Conteúdo / vídeo | `empolgado` |
| `moeda_chocolate` | Negócio / dinheiro | `empolgado` |
| `biscoito_sorte` | Ideia com prazo (vira lembrete) | `empolgado` |
| `minhoca` | Sem pé nem cabeça | `debochando` |
| `comida_estragada` | Ideia ruim | `passando_mal` |

---

## Regras do jogo (no servidor)

- Fome (hp) de 100 a 0, cai ~4 por hora. Ideia boa **+30**, ruim **+5** e `passando_mal` 10 min.
- Fome em 0 por **12 h** → `morto`.
- Ideia com prazo ("me lembra de X até sexta") vira **lembrete**; vencido → **cocô** na tela até marcar como feito.

## Morte e ressurreição

A cada morte o pato **sorteia** o que exige pra voltar (salvo no Core2, reiniciar não muda), e a tela avisa:

| Exige | Dica na tela | Como |
|---|---|---|
| **Massagem cardíaca** | ❤ pulando + "faça massagem cardíaca!" | 30 chacoalhadas a 100–120/min. Toca "Stayin' Alive" (103 bpm) subindo o volume + "tum-tum" a cada batida. Luz verde = no ritmo, laranja = quase, vermelha = recomeça. |
| **Súplica** | 🎤 pulando + "segure o microfone e implore!" | Segura C e implora. Vai pro `/reviver.php` como `desculpa`; o Claude julga. |

- Aceitou → revive `empolgado` com fome 50. Recusou → continua morto, quack choroso.
- A outra forma fica desligada (microfone só vibra / chacoalhar não faz nada).
- "Stayin' Alive" e o coração ficam em `assets/sons-privados/` (**fora do git**: direitos autorais).

## Alexa (back-end PHP) — ainda não feito

| Função | Como |
|---|---|
| **Dedo-duro** | Fome baixa, passando mal ou morte → Alexa anuncia pra casa (Voice Monkey) |
| **Funeral** | Pato morre → música fúnebre + obituário do Claude citando a última ideia |
| **Alimentar pela Alexa** | "Alexa, fala pro pato: …" → skill custom → `POST /ideia.php` com texto |

---

## Etapas = passos do vídeo

| # | Entrega | Status |
|---|---|---|
| 0 | Setup (Core2 no Mac, upload pelo cabo) | ✅ |
| 1 | **Pato vivo na tela** (animações, sons, luzes, vibração) | ✅ |
| 2 | **Pato me ouve** (segura C, grava, manda pro servidor) | ✅ |
| 3 | **Comida + reação** (corte pra comida, come, reage) | ✅ |
| 4 | **Lembrete vira cocô** (tarefas com checkbox, cocô some ao marcar) | ✅ testar com lembretes reais |
| 5 | **Deploy VPS** + hotspot do celular | ✅ servidor no ar · ⏳ testar hotspot |
| 6 | **Semana real** (fome caindo, morte, ressurreição) | ⏳ |

## O que falta

**Firmware**
- [ ] Animação "quase reviveu" (abre o olho e morre de novo) quando a súplica é recusada — aguardando desenho
- [ ] Usar o estado que o `/marcar-feito.php` já devolve (economiza uma consulta)
- [ ] Ajustar força/ritmo do tonto e da massagem depois de testar mais

**Testes**
- [ ] Súplica aceita de ponta a ponta
- [ ] Lembrete real: ideia com prazo → cocô quando vence → marcar → cocô some
- [ ] Hotspot do celular (`WIFI2_SSID`/`WIFI3_SSID` no `config.h`)
- [ ] Bateria: quantas horas aguenta fora da tomada
- [ ] Pato dormindo 21h–7h

**Back-end (parceiro)**
- [ ] Transcrição mais rápida (hoje 33–41 s por ideia; meta < 10 s)
- [ ] Página de ideias (`/ideias`) pra ver tudo que o pato comeu
- [ ] Alexa: dedo-duro, funeral, alimentar por voz
- [ ] (Opcional) servidor decidir a exigência de reviver, pra forçar uma das duas na gravação

**Vídeo**
- [ ] Lembrete diário de gravar o pato
- [ ] Fotos da thumb (Core2 perto da lente, LED vermelho)
- [ ] Espelho (`tools/espelho/`) com fundo verde pra gravar a tela limpa; PNGs dos sprites pra edição
- [ ] Ver Content ID do "Stayin' Alive" no YouTube

---

## Divisão

- **Gabriel + Claude — firmware (Core2):** tudo em `firmware/`, `tools/`, `assets/`.
- **Parceiro — back-end PHP:** endpoints, whisper, prompt do Claude, banco, cron, deploy, Alexa.

## Riscos conhecidos

- WiFi do Core2 é **só 2.4 GHz** → iPhone em "Maximizar Compatibilidade".
- Microfone do Core2 é fraco: falar perto, lugar silencioso.
- Tela reflete na câmera: usar o espelho ou os PNGs na edição.
- A porta USB só aceita um programa: fechar a aba do espelho antes de `pio run -t upload`.
- Abrir a porta USB (espelho ou log) reinicia o Core2.
