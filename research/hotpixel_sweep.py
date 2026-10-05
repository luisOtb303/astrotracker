#!/usr/bin/env python3
"""Barrido de umbral y analisis de falsos positivos.

Para cada umbral k indica cuantos positivos conocidos se detectan y cuantos
falsos positivos aparecen. Ademas caracteriza los falsos positivos: si se
concentran junto a zonas brillantes son estrellas (falso positivo grave), y si
estan dispersos por el fondo son ruido o defectos weaker.

Uso:
    python research/hotpixel_sweep.py imagen.CR2 dx,dy px,py ...
"""

import sys

import numpy as np
import rawpy

OFFSET = (10, 4)
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
    mpx = (h * w) / 1e6
    print(f"{path}: {w}x{h} ({mpx:.1f} Mpx)  patron={pattern.tolist()}")

    # ---- Una sola pasada: calculo todos los campos por paridad ----
    campos = []
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
            sigma = np.maximum(1.4826 * mad, 1.0)
            z = (sub - med) / sigma
            nb_max = st.max(axis=0)
            campos.append((py, px, sub, med, sigma, z, nb_max))
    del st

    def deteccion(k, aislamiento):
        full = np.zeros((h, w), bool)
        for (py, px, sub, med, sigma, z, nb_max) in campos:
            cand = z > k
            if aislamiento:
                cand = cand & (nb_max <= med + k * sigma)
            full[py::2, px::2] = cand
        return full

    # ---- Positivos: localizar el defecto real cerca de cada punto ----
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
                    z = float(f[5][y // 2, x // 2])
                    if best is None or z > best[0]:
                        best = (z, x, y)
        if best[0] < 10:
            print(f"  punto JPEG({jx},{jy}) -> no es hot pixel (z={best[0]:.1f})")
        else:
            print(f"  punto JPEG({jx},{jy}) -> RAW({best[1]},{best[2]}) "
                  f"z={best[0]:.1f}")
            positivos.append((best[1], best[2]))

    # ---- Brillo local en mosaico, para juzgar si un FP es una estrella ----
    pad = np.pad(mosaic, 8, mode="reflect")
    stack = np.stack([pad[8 + dy:8 + dy + h, 8 + dx:8 + dx + w]
                      for dy in (-8, 0, 8) for dx in (-8, 0, 8)], axis=0)
    fondo = np.median(stack, axis=0)
    del stack

    print("\n%6s %10s %12s %12s %s" % ("k", "detectados", "FP/Mpx",
                                       "en hallazgo", "positivos"))
    for k in (5, 6, 8, 10, 12, 15, 20, 25, 30):
        for iso in (False, True):
            full = deteccion(k, iso)
            n = int(full.sum())
            hits = sum(1 for (x, y) in positivos if full[y, x])
            # "en hallazgo" = detecciones cuya ventana tiene brillo de fondo alto
            ys, xs = np.nonzero(full)
            if len(ys):
                sel = fondo[ys, xs] > np.percentile(fondo, 99.0)
                frac = float(sel.mean())
            else:
                frac = 0.0
            print("%6d %10d %12.1f %11.1f%% %d/%d %s"
                  % (k, n, n / mpx, 100 * frac, hits, len(positivos),
                     "(aislado)" if iso else ""))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())