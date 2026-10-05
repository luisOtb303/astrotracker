#!/usr/bin/env python3
"""Volca el mosaico Bayer crudo alrededor de unas coordenadas.

Es la version "antes de debayer" del analisis: en el RAW el defecto es un
unico photosite, mientras que en el JPEG de camara ya aparece manchado en un
2x2. Sirve para confirmar que las coordenadas que da el usuario apuntan al
defecto real y para ver de que color es cada photosite.

Uso:
    python research/hotpixel_raw.py imagen.CR2 1102,926 ...
"""

import sys

import numpy as np
import rawpy

RADIUS = 3


def load_mosaic(path):
    with rawpy.imread(path) as raw:
        vis = raw.raw_image_visible
        pattern = np.array(raw.raw_pattern)          # p.ej. [[0,1],[1,2]]
        colors = np.array(raw.raw_colors_visible)    # RGB por posicion CFA
        return vis.astype(np.uint32), pattern, colors


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    path = sys.argv[1]
    points = [tuple(int(v) for v in a.split(",")) for a in sys.argv[2:]]

    mosaic, pattern, colors = load_mosaic(path)
    h, w = mosaic.shape
    print(f"{path}: {w}x{h}")
    print(f"patron CFA = {pattern.tolist()}  colores = {colors.tolist()}")

    for (x, y) in points:
        print(f"\n=== punto declarado ({x},{y}) ===")
        y0, y1 = max(0, y - RADIUS), min(h, y + RADIUS + 1)
        x0, x1 = max(0, x - RADIUS), min(w, x + RADIUS + 1)
        sub = mosaic[y0:y1, x0:x1]
        cfa = pattern[np.arange(y0, y1)[:, None] % 2, np.arange(x0, x1)[None, :] % 2]

        med = np.median(sub)
        print(f"mediana del parche: {med:.0f}")
        print("valores / color CFA:")
        for row in range(sub.shape[0]):
            cells = []
            for col in range(sub.shape[1]):
                cells.append(f"{sub[row, col]:5d}c{cfa[row, col]}")
            print("  " + " ".join(cells))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())