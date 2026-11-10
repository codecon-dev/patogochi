# Patogochi 🦆

Tamagotchi de pato no M5Stack Core2 que se alimenta de ideias. Plano completo em [PLANO.md](PLANO.md).

## Estrutura

- `assets/sprites/` — PNGs do pato (32x32), cocô e comidas
- `tools/gen_sprites.py` — converte os PNGs em arrays RGB565 pro firmware
- `firmware/` — código do Core2 (PlatformIO + M5Unified)

## Rodar o firmware

```sh
brew install platformio ffmpeg
python3 tools/gen_sprites.py          # só se mudar algum PNG
cd firmware
pio run                               # compila
pio run -t upload                     # compila e grava no Core2 (cabo USB-C)
```

A porta do Core2 está em `firmware/platformio.ini` (`upload_port`). Pra achar a sua: `ls /dev/cu.*`.

## Botões (modo debug)

A = estado anterior · B = põe/tira cocô · C = próximo estado
