#!/usr/bin/env python3
"""Converte os PNGs de assets/sprites em arrays RGB565 pro firmware.

Uso (da raiz do repo):  python3 tools/gen_sprites.py
Precisa do ffmpeg instalado. Gera firmware/src/sprites_data.cpp e firmware/include/sprites.h.
Pixel transparente vira SPRITE_TRANSPARENT (magenta 0xF81F).
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SPRITES = ROOT / "assets" / "sprites"
OUT_CPP = ROOT / "firmware" / "src" / "sprites_data.cpp"
OUT_H = ROOT / "firmware" / "include" / "sprites.h"
TRANSPARENT = 0xF81F

# (pasta, nome no código) — animações do pato, na ordem do enum
ANIMS = [
    ("parado", "PARADO"),
    ("ouvindo", "OUVINDO"),
    ("pensando", "PENSANDO"),
    ("comendo", "COMENDO"),
    ("empolgado", "EMPOLGADO"),
    ("debochando", "DEBOCHANDO"),
    ("passando_mal", "PASSANDO_MAL"),
    ("triste_fome", "TRISTE_FOME"),
    ("dormindo", "DORMINDO"),
    ("morto", "MORTO"),
    ("coco", "COCO"),
]
COMIDAS = [
    ("pipoca", "PIPOCA"),
    ("moeda_chocolate", "MOEDA_CHOCOLATE"),
    ("minhoca", "MINHOCA"),
    ("biscoito_sorte", "BISCOITO_SORTE"),
    ("comida_estragada", "COMIDA_ESTRAGADA"),
]


def size_of(png):
    out = subprocess.check_output(
        ["ffprobe", "-v", "error", "-show_entries", "stream=width,height",
         "-of", "csv=p=0", str(png)], text=True).strip()
    w, h = out.split(",")
    return int(w), int(h)


def rgb565(png):
    w, h = size_of(png)
    raw = subprocess.check_output(
        ["ffmpeg", "-v", "error", "-i", str(png), "-f", "rawvideo",
         "-pix_fmt", "rgba", "-"])
    px = []
    for i in range(0, len(raw), 4):
        r, g, b, a = raw[i:i + 4]
        if a < 128:
            px.append(TRANSPARENT)
            continue
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        px.append(v ^ 1 if v == TRANSPARENT else v)  # evita colisão com a cor-chave
    return w, h, px


def emit(name, frames_px, w, h):
    lines = [f"static const uint16_t {name}_DATA[] = {{"]
    for px in frames_px:
        for row in range(h):
            chunk = px[row * w:(row + 1) * w]
            lines.append("  " + ",".join(f"0x{v:04X}" for v in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    cpp = ['// GERADO por tools/gen_sprites.py — não editar à mão', '#include "sprites.h"', ""]
    anim_entries, food_entries = [], []
    total = 0
    for folder, name in ANIMS + COMIDAS:
        is_food = (folder, name) in COMIDAS
        files = [SPRITES / "comidas" / f"{folder}.png"] if is_food else sorted((SPRITES / folder).glob("*.png"))
        if not files:
            sys.exit(f"sem frames em {folder}")
        frames, size = [], None
        for f in files:
            w, h, px = rgb565(f)
            if size and size != (w, h):
                sys.exit(f"{f}: tamanho {w}x{h} diferente de {size}")
            size = (w, h)
            frames.append(px)
        w, h = size
        cpp.append(emit(name, frames, w, h))
        entry = f'  {{"{folder}", {w}, {h}, {len(frames)}, {name}_DATA}},'
        (food_entries if is_food else anim_entries).append(entry)
        total += len(frames) * w * h * 2
        print(f"{folder:18} {len(frames):3} frames {w}x{h}")
    cpp.append("\nconst Sprite ANIMS[ANIM_COUNT] = {\n" + "\n".join(anim_entries) + "\n};")
    cpp.append("\nconst Sprite FOODS[FOOD_COUNT] = {\n" + "\n".join(food_entries) + "\n};\n")
    OUT_CPP.write_text("\n".join(cpp))

    h = ['// GERADO por tools/gen_sprites.py — não editar à mão', "#pragma once", "#include <stdint.h>", "",
         f"#define SPRITE_TRANSPARENT 0x{TRANSPARENT:04X}", "",
         "struct Sprite {", "  const char* name;", "  uint8_t w, h;", "  uint16_t frames;",
         "  const uint16_t* data;  // frames * w * h pixels RGB565", "};", "",
         "enum Anim : uint8_t {"]
    h += [f"  ANIM_{n}," for _, n in ANIMS] + ["  ANIM_COUNT", "};", "", "enum Food : uint8_t {"]
    h += [f"  FOOD_{n}," for _, n in COMIDAS] + ["  FOOD_COUNT", "};", "",
          "extern const Sprite ANIMS[ANIM_COUNT];", "extern const Sprite FOODS[FOOD_COUNT];", ""]
    OUT_H.parent.mkdir(exist_ok=True)
    OUT_H.write_text("\n".join(h))
    print(f"total: {total / 1024:.0f} KB em flash")


if __name__ == "__main__":
    main()
