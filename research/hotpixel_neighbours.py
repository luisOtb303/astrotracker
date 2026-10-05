#!/usr/bin/env python3
"""Analisis numerico del entorno de las coordenadas indicadas.

Volca un mapa de texto del vecindario (canal mas alto por pixel, escala 0..9)
para ver la estructura del defecto, y ademas busca el outlier mas fuerte en
una ventana alrededor de cada punto para deducir si la coordenada dada apunta
al centro del defecto o hay que desplazarla.

Uso:
    python research/hotpixel_neighbours.py imagen.png 1102,926 ...
"""

import sys

import cv2
import numpy as np

WINDOW = 6
SCALE = 12


def ascii_map(patch: np.ndarray, peak: float) -> str:
    chars = "0123456789"
    lines = []
    for row in patch:
        line = ""
        for v in row:
            idx = int(min(8, round(v / peak * 8))) if peak > 0 else 0
            line += chars[idx]
        lines.append(line)
    return "\n".join(lines)


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    path = sys.argv[1]
    points = [tuple(int(v) for v in a.split(",")) for a in sys.argv[2:]]

    bgr = cv2.imread(path, cv2.IMREAD_COLOR)
    if bgr is None:
        print(f"no se pudo leer {path}")
        return 1
    h, w = bgr.shape[:2]
    print(f"{path}: {w}x{h}  (canal mas alto por pixel, 0..{SCALE})")

    for (px, py) in points:
        print(f"\n=== punto declarado ({px},{py}) ===")

        # 1. Vecindario inmediato.
        x0, x1 = max(0, px - 4), min(w, px + 5)
        y0, y1 = max(0, py - 4), min(h, py + 5)
        patch = bgr[y0:y1, x0:x1].max(axis=2).astype(np.float32)
        med = np.median(patch)
        print(f"mediana del vecindario: {med:.1f}")
        print(ascii_map(patch, max(med * 2.5, patch.max())))

        # 2. Outlier mas fuerte en una ventana mayor: los defectusbien visibles
        #    suelen estarcentrados en el punto, pero conviene comprobarlo.
        wx0, wx1 = max(0, px - WINDOW), min(w, px + WINDOW + 1)
        wy0, wy1 = max(0, py - WINDOW), min(h, py + WINDOW + 1)
        win = bgr[wy0:wy1, wx0:wx1].max(axis=2).astype(np.float32)
        loc = np.unravel_index(np.argmax(win), win.shape)
        wy, wx = int(loc[0]) + wy0, int(loc[1]) + wx0
        print(f"maximo en ventana +-{WINDOW}: ({wx},{wy}) "
              f"valor={win[loc]:.0f}  delta={win[loc] - med:+.0f}  "
              f"desplazamiento=({wx - px:+d},{wy - py:+d})")

        # 3. Delta real con mediana 3x3 calculada bien sobre la imagen entera.
        med3 = cv2.medianBlur(bgr, 3)
        delta = np.abs(bgr[py, px].astype(np.float32) -
                       med3[py, px].astype(np.float32))
        print(f"delta 3x3 en el punto: {delta.max():.0f} "
              f"(canales BGR={[int(v) for v in delta]})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())