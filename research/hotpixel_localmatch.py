#!/usr/bin/env python3
"""Correlaciona un parche del JPEG contra el mosaico RAW en su entorno.

Sirve para dos cosas:
  1. medir el desplazamiento LOCAL de esa zona (por si el offset global no aplica);
  2. decidir si el punto brillante que ve el usuario tiene un photosito hot
     debajo, o si lo que hay en el RAW es otra cosa.

Se normaliza el contraste antes de correlacionar, porque el RAW es lineal y el
JPEG lleva gamma, tono y compresion.

Uso:
    python research/hotpixel_localmatch.py imagen.CR2 imagen.jpg 1102,926 ...
"""

import sys

import cv2
import numpy as np
import rawpy

HALF = 10
BUSQUEDA = 40


def norm(a):
    a = a.astype(np.float32)
    sd = a.std()
    return (a - a.mean()) / (sd if sd > 1e-6 else 1.0)


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    raw_path, jpg_path = sys.argv[1], sys.argv[2]
    points = [tuple(int(v) for v in a.split(",")) for a in sys.argv[3:]]

    with rawpy.imread(raw_path) as raw:
        mosaic = raw.raw_image_visible.astype(np.float32)
        pattern = np.array(raw.raw_pattern)
    h, w = mosaic.shape

    jpg = cv2.imread(jpg_path, cv2.IMREAD_COLOR)
    jh, jw = jpg.shape[:2]
    jl = jpg.astype(np.float32).mean(axis=2)
    # El JPEG lleva gamma; el RAW no. Aplicar una gamma parecida al look canonico
    # hace que la correlacion no se vaya al gris.
    jg = np.power(np.clip(jl / 255.0, 0, 1), 1 / 2.2) * 255.0

    for (px, py) in points:
        print(f"\n=== JPEG({px},{py}) ===")
        if not (HALF <= px < jw - HALF and HALF <= py < jh - HALF):
            print("  cerca del borde")
            continue

        tpl = norm(jg[py - HALF:py + HALF + 1, px - HALF:px + HALF + 1])

        # Region del RAW donde puede estar, ya descentrada por el offset global.
        cx, cy = px + 9, py + 3
        best = None
        for dy in range(-BUSQUEDA, BUSQUEDA + 1, 1):
            for dx in range(-BUSQUEDA, BUSQUEDA + 1, 1):
                x0, y0 = cx + dx - HALF, cy + dy - HALF
                if x0 < 0 or y0 < 0 or x0 + 2 * HALF + 1 > w or y0 + 2 * HALF + 1 > h:
                    continue
                cand = norm(mosaic[y0:y0 + 2 * HALF + 1, x0:x0 + 2 * HALF + 1])
                score = float((cand * tpl).mean())
                if best is None or score > best[0]:
                    best = (score, cx + dx, cy + dy)

        score, bx, by = best
        print(f"  mejor correlacion={score:5.3f}  en RAW({bx},{by})  "
              f"-> offset local=({bx - px:+d},{by - py:+d})")

        # El punto central, ya situado.
        cfa = pattern[by % 2, bx % 2]
        patch = mosaic[by - 2:by + 3, bx - 2:bx + 3]
        print(f"  photosite={mosaic[by, bx]:.0f} cfa={cfa}  "
              f"mediana 5x5={np.median(patch):.0f}  max 5x5={patch.max():.0f}")

        # Y el valor equivalente en el JPEG, para contrastar.
        print(f"  valor en JPEG={jl[py, px]:.0f}  "
              f"mediana 5x5={np.median(jl[py - 2:py + 3, px - 2:px + 3]):.0f}  "
              f"max 5x5={jl[py - 2:py + 3, px - 2:px + 3].max():.0f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())