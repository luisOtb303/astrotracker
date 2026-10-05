#!/usr/bin/env python3
"""Recorta y amplia las zonas indicadas para ver los pixeles a ojo.

Herramienta de calibracion: escribe un PNG por cada coordenada para poder
inspeccionar como es el defecto y a que distancia esta del fondo.

Uso:
    python research/hotpixel_crop.py salida_dir imagen.png 1102,926 ...
"""

import os
import sys

import cv2
import numpy as np

HALF = 12      # radio del recorte en pixeles
ZOOM = 12      # ampliacion


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    outdir = sys.argv[1]
    path = sys.argv[2]
    points = [tuple(int(v) for v in a.split(",")) for a in sys.argv[3:]]

    bgr = cv2.imread(path, cv2.IMREAD_COLOR)
    if bgr is None:
        print(f"no se pudo leer {path}")
        return 1
    h, w = bgr.shape[:2]
    os.makedirs(outdir, exist_ok=True)
    stem = os.path.splitext(os.path.basename(path))[0]
    print(f"{path}: {w}x{h}")

    for (x, y) in points:
        x0, x1 = max(0, x - HALF), min(w, x + HALF + 1)
        y0, y1 = max(0, y - HALF), min(h, y + HALF + 1)
        crop = bgr[y0:y1, x0:x1]

        big = cv2.resize(crop, None, fx=ZOOM, fy=ZOOM,
                         interpolation=cv2.INTER_NEAREST)
        # Cruz sobre el pixel central (el del punto pedido).
        cx, cy = (x - x0) * ZOOM + ZOOM // 2, (y - y0) * ZOOM + ZOOM // 2
        cv2.line(big, (cx - ZOOM * 2, cy), (cx - 2, cy), (0, 0, 255), 1)
        cv2.line(big, (cx + 2, cy), (cx + ZOOM * 2, cy), (0, 0, 255), 1)
        cv2.line(big, (cx, cy - ZOOM * 2), (cx, cy - 2), (0, 0, 255), 1)
        cv2.line(big, (cx, cy + 2), (cx, cy + ZOOM * 2), (0, 0, 255), 1)

        out = os.path.join(outdir, f"{stem}_{x}_{y}.png")
        cv2.imwrite(out, big)
        print(f"  ({x},{y}) valor={bgr[y, x].tolist()} -> {out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())