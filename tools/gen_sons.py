#!/usr/bin/env python3
"""Converte os WAVs de assets/sons em arrays PCM 16 bits (16 kHz, mono) pro firmware.

Uso (da raiz do repo):  python3 tools/gen_sons.py
Gera firmware/src/sons_data.cpp e firmware/include/sons.h.
"""
import pathlib
import struct
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
SONS = ROOT / "assets" / "sons"
OUT_CPP = ROOT / "firmware" / "src" / "sons_data.cpp"
OUT_H = ROOT / "firmware" / "include" / "sons.h"
# Sons com direitos autorais (música) ficam fora do git: assets/sons-privados/ gera
# sons_privados_data.cpp / sons_privados.h, também fora do git. Sem a pasta, o firmware compila sem eles.
PRIV = ROOT / "assets" / "sons-privados"
PRIV_CPP = ROOT / "firmware" / "src" / "sons_privados_data.cpp"
PRIV_H = ROOT / "firmware" / "include" / "sons_privados.h"
RATE = 16000


def gera(pasta, out_cpp, out_h, header):
    cpp = ["// GERADO por tools/gen_sons.py — não editar à mão", f'#include "{header}"', ""]
    h = ["// GERADO por tools/gen_sons.py — não editar à mão", "#pragma once", "#include <stddef.h>",
         "#include <stdint.h>", "", f"#define SOM_SAMPLE_RATE {RATE}", ""]
    for wav in sorted(pasta.glob("*.wav")):
        nome = wav.stem.upper()
        raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(wav), "-f", "s16le",
                                       "-ac", "1", "-ar", str(RATE), "-"])
        amostras = struct.unpack(f"<{len(raw) // 2}h", raw)
        linhas = [", ".join(str(v) for v in amostras[i:i + 16]) for i in range(0, len(amostras), 16)]
        cpp.append(f"const int16_t SOM_{nome}[] = {{\n  " + ",\n  ".join(linhas) + "\n};")
        cpp.append(f"const size_t SOM_{nome}_LEN = {len(amostras)};\n")
        h.append(f"extern const int16_t SOM_{nome}[];")
        h.append(f"extern const size_t SOM_{nome}_LEN;")
        print(f"{wav.name}: {len(amostras)} amostras ({len(amostras) / RATE:.2f}s)")
    out_cpp.write_text("\n".join(cpp) + "\n")
    out_h.write_text("\n".join(h) + "\n")


def main():
    gera(SONS, OUT_CPP, OUT_H, "sons.h")
    if PRIV.is_dir() and any(PRIV.glob("*.wav")):
        gera(PRIV, PRIV_CPP, PRIV_H, "sons_privados.h")
    else:
        for f in (PRIV_CPP, PRIV_H):
            f.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
