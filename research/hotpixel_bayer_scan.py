#!/usr/bin/env python3
"""Detector Bayer-aware de fotositos calientes sobre el mosaico RAW.

Metodo:
  1. El mosaico se parte en 4 sub-imagenes por paridad CFA, de modo que cada
     una contiene un unico color y el "vecindario del mismo color" es simply
     un 3x3 (o 5x5) sin el centro sobre la sub-imagen.
  2. Por cada photosite: mediana y MAD de sus vecinos del MISMO color.
  3. z = (valor - mediana) / (1.4826 * MAD), con suelo para evitar division
     por cero en zonas planas.
  4. Exige ademas aislamiento: que ningun vecino del mismo color sea a su vez
     un outlier. Una estrella (tras debayer) no lo cumple, un fotosito si.

Sirve para encontrar la verdad de terreno y medir falsos positivos.

Uso:
    python research/hotpixel_bayer_scan.py imagen.CR2 [sensibilidad]
"""

import sys

import cv2
import numpy as np
import rawpy

# Radio en la sub-imagen (= radio/2 en el mosaico original).
RADIUS = 2


def load_mosaic(path):
    with rawpy.imread(path) as raw:
        vis = raw.raw_image_visible.astype(np.float32)
        pattern = np.array(raw.raw_pattern)
        return vis, pattern


def shifted_stack(sub, radius):
    """Apila los vecinos del mismo color: (K, h, w) sin el centro."""
    pad = np.pad(sub, radius, mode="reflect")
    h, w = sub.shape
    views = []
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            if dy == 0 and dx == 0:
                continue
            views.append(pad[radius + dy:radius + dy + h,
                             radius + dx:radius + dx + w])
    return np.stack(views, axis=0)


def analyse_sub(sub, k_sigma):
    radius = RADIUS
    st = shifted_stack(sub, radius)

    med = np.median(st, axis=0)
    mad = np.median(np.abs(st - med), axis=0)

    # Sigma robusto local (MAD escalado) y suelo: en zonas planas el MAD es 0.
    sigma = np.maximum(1.4826 * mad, k_sigma * 0.5)
    z = (sub - med) / sigma

    cand = z > k_sigma

    # Aislamiento: ningun vecino del mismo color es a su vez outlier.
    nb_max = st.max(axis=0)
    isolated = cand & (nb_max <= med + k_sigma * sigma)
    return med, sigma, z, cand, isolated


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    path = sys.argv[1]
    ks = [float(v) for v in sys.argv[2:]] or [4.0, 6.0, 8.0, 12.0]

    mosaic, pattern = load_mosaic(path)
    h, w = mosaic.shape
    print(f"{path}: {w}x{h}  patron={pattern.tolist()}")

    # Etiqueta CFA por paridad.
    yy, xx = np.mgrid[0:h, 0:w]
    cfa = pattern[yy % 2, xx % 2]

    for k in ks:
        total = 0
        total_iso = 0
        best = []
        for py in range(2):
            for px in range(2):
                sub = mosaic[py::2, px::2]
                if sub.size == 0:
                    continue
                med, sigma, z, cand, iso = analyse_sub(sub, k)
                total += int(cand.sum())
                total_iso += int(iso.sum())
                if iso.any():
                    zs = np.where(iso, z, -np.inf)
                    for idx in np.argpartition(zs.ravel(), -5)[-5:]:
                        sy, sx = np.unravel_index(idx, zs.shape)
                        best.append((float(zs[sy, sx]),
                                     px + 2 * int(sx), py + 2 * int(sy)))
        best.sort(reverse=True)
        print(f"\nk={k:5.1f} sigma  candidatos={total:8d}  aislados={total_iso:7d}"
              f"  ({100.0 * total_iso / (h * w):.5f}% del mosaico)")
        for (z, x, y) in best[:5]:
            print(f"   ({x:5d},{y:5d})  z={z:8.1f}  cfa={cfa[y, x]}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())