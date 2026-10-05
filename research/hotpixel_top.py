#!/usr/bin/env python3
"""Lista las detecciones mas fuertes y su distancia al punto declarado.

Sirve para comprobar si el punto que dio el usuario esta cerca de un defecto
real (error de lectura de la coordenada) o si no hay nada cerca (o el defecto no
es un fotosito del sensor).

Uso:
    python research/hotpixel_top.py imagen.CR2 dx,dy px,py ...
"""

import sys

import numpy as np
import rawpy

RADIO = 3


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    path = sys.argv[1]
    offx, offy = (int(v) for v in sys.argv[2].split(","))
    pts = [tuple(int(v) for v in a.split(",")) for a in sys.argv[3:]]

    with rawpy.imread(path) as raw:
        mosaic = raw.raw_image_visible.astype(np.float32)
        pattern = np.array(raw.raw_pattern)
    h, w = mosaic.shape

    zfull = np.zeros((h, w), np.float32)
    for py in range(2):
        for px in range(2):
            sub = mosaic[py::2, px::2]
            pad = np.pad(sub, RADIO, mode="reflect")
            hs, ws = sub.shape
            st = np.stack([pad[RADIO + dy:RADIO + dy + hs,
                               RADIO + dx:RADIO + dx + ws]
                           for dy in range(-RADIO, RADIO + 1)
                           for dx in range(-RADIO, RADIO + 1)
                           if dy or dx], axis=0)
            med = np.median(st, axis=0)
            mad = np.median(np.abs(st - med), axis=0)
            zfull[py::2, px::2] = (sub - med) / np.maximum(1.4826 * mad, 1.0)
            del st

    thr = 20.0
    ys, xs = np.nonzero(zfull > thr)
    zs = zfull[ys, xs]
    order = np.argsort(-zs)
    print(f"{path}: {len(ys)} detecciones con z>{thr} "
          f"({len(ys) / (h * w) * 1e6:.1f} por Mpx)")

    print("\ntop 15:")
    for i in order[:15]:
        x, y, z = int(xs[i]), int(ys[i]), float(zs[i])
        d = ""
        for (jx, jy) in pts:
            dist = np.hypot(x - (jx + offx), y - (jy + offy))
            d += f"  dist_a({jx},{jy})={dist:6.0f}px"
        print(f"  RAW({x:5d},{y:5d}) cfa={pattern[y % 2, x % 2]} z={z:8.1f}"
              f"  valor={mosaic[y, x]:7.0f}{d}")

    # Vecindario del punto declarado: nombres de colores y valores.
    for (jx, jy) in pts:
        cx, cy = jx + offx, jy + offy
        print(f"\n=== punto declarado JPEG({jx},{jy}) -> RAW({cx},{cy}) ===")
        y0, y1 = max(0, cy - 3), min(h, cy + 4)
        x0, x1 = max(0, cx - 3), min(w, cx + 4)
        sub = mosaic[y0:y1, x0:x1]
        cfa = pattern[np.arange(y0, y1)[:, None] % 2,
                      np.arange(x0, x1)[None, :] % 2]
        print("  valores (marcado * el photosite central):")
        for r in range(sub.shape[0]):
            cells = []
            for c in range(sub.shape[1]):
                mark = "*" if (y0 + r == cy and x0 + c == cx) else " "
                cells.append(f"{mark}{sub[r, c]:5.0f}c{cfa[r, c]}")
            print("   " + " ".join(cells))
        print(f"  max en la ventana={sub.max():.0f}  "
              f"mediana={np.median(sub):.0f}  z={zfull[cy, cx]:.1f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())