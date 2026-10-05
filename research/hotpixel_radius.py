#!/usr/bin/env python3
"""Comprueba si el radio del vecindario cambia el resultado.

Las mediciones de docs/hotpixel-filter.md se hicieron con radio 3 sobre la
sub-imagen (48 vecinos del mismo color, +-6 px del mosaico), pero el documento
anota radio 2. Esto mide ambos (y radio 1, el mas barato: medianBlur k=3) para
decidir con que radio se implementa.

Uso:
    python research/hotpixel_radius.py imagen.CR2 dx,dy px,py ...
"""

import sys

import numpy as np
import rawpy


def vistas(sub, radio):
    pad = np.pad(sub, radio, mode="reflect")
    h, w = sub.shape
    return np.stack([pad[radio + dy:radio + dy + h, radio + dx:radio + dx + w]
                     for dy in range(-radio, radio + 1)
                     for dx in range(-radio, radio + 1)
                     if dy or dx], axis=0)


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    path = sys.argv[1]
    offx, offy = (int(v) for v in sys.argv[2].split(","))
    pts = [tuple(int(v) for v in a.split(",")) for a in sys.argv[3:]]

    with rawpy.imread(path) as raw:
        mosaic = raw.raw_image_visible.astype(np.float32)
    h, w = mosaic.shape
    mpx = (h * w) / 1e6
    print(f"{path}: {w}x{h} ({mpx:.1f} Mpx)")

    for radio in (1, 2, 3):
        campos = []
        for py in range(2):
            for px in range(2):
                sub = mosaic[py::2, px::2]
                st = vistas(sub, radio)
                med = np.median(st, axis=0)
                mad = np.median(np.abs(st - med), axis=0)
                sigma = np.maximum(1.4826 * mad, 1.0)
                z = (sub - med) / sigma
                nb_max = st.max(axis=0)
                campos.append((py, px, z, med, sigma, nb_max))
                del st

        vecinos = (2 * radio + 1) ** 2 - 1
        alcance = radio * 2
        print(f"\n--- radio {radio} en sub-imagen: {vecinos} vecinos del mismo "
              f"color, alcance +-{alcance} px del mosaico ---")

        def deteccion(k):
            full = np.zeros((h, w), bool)
            for (py, px, z, med, sigma, nb_max) in campos:
                c = z > k
                c = c & (nb_max <= med + k * sigma)
                full[py::2, px::2] = c
            return full

        positivos = []
        for (jx, jy) in pts:
            cx, cy = jx + offx, jy + offy
            best = None
            for dy in range(-3, 4):
                for dx in range(-3, 4):
                    x, y = cx + dx, cy + dy
                    if 0 <= x < w and 0 <= y < h:
                        py, px = y % 2, x % 2
                        f = [c for c in campos if c[0] == py and c[1] == px][0]
                        zz = float(f[2][y // 2, x // 2])
                        if best is None or zz > best[0]:
                            best = (zz, x, y)
            if best[0] >= 10:
                positivos.append((best[1], best[2]))
            print(f"    punto JPEG({jx},{jy}) -> z={best[0]:.1f}")

        for k in (8, 12, 15, 20, 30):
            full = deteccion(k)
            n = int(full.sum())
            hits = sum(1 for (x, y) in positivos if full[y, x])
            print("    k=%3d  detectados=%6d  FP/Mpx=%6.1f  positivos=%d/%d"
                  % (k, n, n / mpx, hits, len(positivos)))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())