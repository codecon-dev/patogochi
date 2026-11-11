# Patogochi 🦆

Tamagotchi de pato no M5Stack Core2 que se alimenta de ideias. Plano completo em [PLANO.md](PLANO.md).

## Estrutura

- `assets/sprites/` — PNGs do pato (32x32), cocô e comidas
- `tools/gen_sprites.py` — converte os PNGs em arrays RGB565 pro firmware
- `firmware/` — código do Core2 (PlatformIO + M5Unified)
- `tools/espelho/` — espelho da tela no Chrome

## Rodar o firmware

```sh
brew install platformio ffmpeg
python3 tools/gen_sprites.py          # só se mudar algum PNG
python3 tools/gen_sons.py             # só se mudar algum WAV em assets/sons
cd firmware
pio run                               # compila
pio run -t upload                     # compila e grava no Core2 (cabo USB-C)
```

A porta do Core2 está em `firmware/platformio.ini` (`upload_port`). Pra achar a sua: `ls /dev/cu.*`.

## Botões

Barra de ícones embaixo da tela, um por botão:

| Tela | A (esquerda) | B (meio) | C (direita) |
|---|---|---|---|
| Pato | 📋 tarefas | ❤️ status | 🎤 segura pra falar (solta envia, arrasta pro lado cancela) |
| Tarefas | ← voltar | ↓ próxima | ✓ marca feita |
| Status | ← voltar | | |

- **Toque no pato:** quack
- **Chacoalhar:** pato vivo fica tonto
- **Pato morto:** a cada morte ele sorteia o que exige, e a tela avisa:
  - *massagem cardíaca*: 30 chacoalhadas a 100-120 por minuto, com "Stayin' Alive" subindo o volume (luz verde = no ritmo, laranja = quase, vermelha = recomeça)
  - *súplica*: segurar C e implorar (vai pro `/reviver.php` como `desculpa`)
- **Despertador:** seg–sex 6:15, quack feliz + luzes + vibração + relógio piscando. Toque na tela ou qualquer botão desliga (para sozinho em 10 min). Teste: com o painel de debug aberto, toque curto no C.
- **Segurar no pato 10 s:** liga/desliga o painel de debug (WiFi, IP, servidor, fome, hora, última gravação/resposta/erro, bateria)

Sozinho: tela fica fraca depois de 1 min sem mexer, pato dorme das 21h até o despertador (sáb/dom até 8h ou chacoalhar; fundo roxo com estrelas) e, na bateria, o Core2 entra em sono profundo (toque na tela acorda), cocôs = lembretes vencidos (vem do servidor), bateria piscando abaixo de 20%.

## WiFi e servidor

```sh
cp firmware/include/config.example.h firmware/include/config.h   # preencha WiFi (2.4 GHz) e URL
```

Dá pra cadastrar até 3 redes (`WIFI2_SSID`, `WIFI3_SSID`...): o Core2 conecta na mais forte por perto e reconecta sozinho.

`config.h` fica fora do git. Os endpoints do back-end PHP estão em [API.md](API.md).

## Espelho da tela no Mac

O Core2 manda o estado do pato pelo USB e uma página no Chrome redesenha a tela com os PNGs (nítido, bom pra gravar).

```sh
python3 -m http.server 8000          # na raiz do repo
```

1. Abra http://localhost:8000/tools/espelho/ no **Chrome** (Safari não tem Web Serial)
2. Clique **Conectar no Core2** e escolha a porta `usbserial`
3. **H** esconde os botões; **Fundo** alterna entre cena e verde (chroma key)

A porta USB só aceita um programa por vez: desconecte a página (feche a aba) antes de `pio run -t upload`.
