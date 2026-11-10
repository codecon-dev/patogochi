# Patogochi 🦆

Tamagotchi de pato no M5Stack Core2 que se alimenta de ideias. Plano completo em [PLANO.md](PLANO.md).

## Estrutura

- `assets/sprites/` — PNGs do pato (32x32), cocô e comidas
- `tools/gen_sprites.py` — converte os PNGs em arrays RGB565 pro firmware
- `firmware/` — código do Core2 (PlatformIO + M5Unified)
- `tools/mock-servidor/` — back-end de mentira em PHP que segue o API.md
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

- **B segurando:** grava uma ideia (solta pra enviar)
- **B toque curto:** põe/tira cocô (debug)
- **A / C:** estado anterior / próximo (debug)
- **Toque no pato:** quack

## WiFi e servidor

```sh
cp firmware/include/config.example.h firmware/include/config.h   # preencha WiFi (2.4 GHz) e URL
```

`config.h` fica fora do git. Contrato da API em [API.md](API.md).

Servidor de mentira pra testar sem o back-end real (salva os áudios em `tools/mock-servidor/gravacoes/`):

```sh
php -S 0.0.0.0:8080 tools/mock-servidor/index.php
```

## Espelho da tela no Mac

O Core2 manda o estado do pato pelo USB e uma página no Chrome redesenha a tela com os PNGs (nítido, bom pra gravar).

```sh
python3 -m http.server 8000          # na raiz do repo
```

1. Abra http://localhost:8000/tools/espelho/ no **Chrome** (Safari não tem Web Serial)
2. Clique **Conectar no Core2** e escolha a porta `usbserial`
3. **H** esconde os botões; **Fundo** alterna entre cena e verde (chroma key)

A porta USB só aceita um programa por vez: desconecte a página (feche a aba) antes de `pio run -t upload`.
