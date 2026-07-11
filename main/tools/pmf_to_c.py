#!/usr/bin/env python3
"""
pmf_to_c.py — convertit un fichier .pmf (musique tracker Gamebuino) en
tableau d'octets C++ compilable, exactement comme les sprites (assets/gfx).

Usage :
    python3 pmf_to_c.py music_hills.pmf assets/music/music_hills.cpp

Le nom de la variable générée est dérivé du nom de fichier .pmf en ENTRÉE
(sans l'extension), donc utilise le même nom que celui attendu par le
registre du jeu (cf. music_registry.h) : music_hills.pmf -> music_hills_pmf.
"""
import sys
import os


def convert(pmf_path: str, out_cpp_path: str):
    with open(pmf_path, "rb") as f:
        data = f.read()

    base = os.path.splitext(os.path.basename(pmf_path))[0]  # ex: music_hills
    var_name = f"{base}_pmf"
    header_name = f"{base}.h"

    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append(",".join(f"0x{b:02X}" for b in chunk) + ",")
    body = "\n".join(lines)

    cpp = (
        f'#include "{header_name}"\n\n'
        f"const uint8_t {var_name}[] = {{\n{body}\n}};\n"
        f"const unsigned int {var_name}_len = {len(data)};\n"
    )

    out_dir = os.path.dirname(out_cpp_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(out_cpp_path, "w") as f:
        f.write(cpp)

    print(f"OK : {pmf_path} ({len(data)} octets) -> {out_cpp_path}  [{var_name}]")

    # Le header n'a pas besoin d'être régénéré (déjà présent dans
    # assets/music/*.h), mais on le montre ici pour référence si tu dois
    # ajouter une NOUVELLE piste qui n'a pas encore son .h :
    expected_header = (
        f"#pragma once\n#include <cstdint>\n\n"
        f"extern const uint8_t {var_name}[];\n"
        f"extern const unsigned int {var_name}_len;\n"
    )
    print("Header attendu (déjà présent normalement) :\n" + expected_header)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
