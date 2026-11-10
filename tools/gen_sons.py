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
RATE = 16000


def main():
    cpp = ["// GERADO por tools/gen_sons.py — não editar à mão", '#include "sons.h"', ""]
    h = ["// GERADO por tools/gen_sons.py — não editar à mão", "#pragma once", "#include <stddef.h>",
         "#include <stdint.h>", "", f"#define SOM_SAMPLE_RATE {RATE}", ""]
    for wav in sorted(SONS.glob("*.wav")):
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
    OUT_CPP.write_text("\n".join(cpp) + "\n")
    OUT_H.write_text("\n".join(h) + "\n")


if __name__ == "__main__":
    main()
