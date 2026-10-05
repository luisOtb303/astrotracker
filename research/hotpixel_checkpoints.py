#!/usr/bin/env python3
"""Traduce coordenadas vistas en el JPEG al mosaico RAW y mide su z-score.

El desplazamiento (9, 3) se ha medido empíricamente sobre outliers inequívocos
del propio RAW (ver hotpixel_crosscheck.py), no estimado a ojo.

Para cada punto imprime el valor del photosite, la mediana y el MAD de sus
vecinos DEL MISMO COLOR, y el z resultante con el criterio de aislamiento.

Uso:
    python research/hotpixel_checkpoints.py imagen.CR2 imagen.jpg 1102,926 ...
"""

import sys

import numpy as np
import rawpy

OFFSET_X = 9
OFFSET_Y = 3
RADIO = 3  # radio en el mosaico original


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
    print(f"{raw_path}: {w}x{h}  patron={pattern.tolist()}")

    for (jx, jy) in points:
        x, y = jx + OFFSET_X, jy + OFFSET_Y
        print(f"\n=== JPEG({jx},{jy}) -> RAW({x},{y}) ===")
        if not (0 <= x < w and 0 <= h):
            print("  fuera de rango")
            continue

        cfa = pattern[y % 2, x % 2]
        y0, y1 = max(0, y - RADIO), min(h, y + RADIO + 1)
        x0, x1 = max(0, x - RADIO), min(w, x + RADIO + 1)

        # Vecinos del mismo color: mismo residuo paridad, excluyendo el centro.
        vals = []
        for yy in range(y0, y1):
            for xx in range(x0, x1):
                if (yy, xx) == (y, x):
                    continue
                if pattern[yy % 2, xx % 2] == cfa:
                    vals.append(mosaic[yy, xx])
        vals = np.array(vals)

        med = float(np.median(vals))
        mad = float(np.median(np.abs(vals - med)))
        sigma = 1.4826 * mad
        val = float(mosaic[y, x])
        z = (val - med) / sigma if sigma > 0 else float("inf")

        print(f"  photosite  valor={val:8.0f}  cfa={cfa}")
        print(f"  vecinos    n={vals.size}  mediana={med:8.0f}  MAD={mad:6.1f}"
              f"  sigma={sigma:6.1f}")
        print(f"  exceso     {val - med:+8.0f}   z={(val - med) / max(sigma, 1.0):7.1f}")
        print(f"  vecinos del mismo color: "
              + " ".join(f"{v:.0f}" for v in sorted(vals, reverse=True)[:8]))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())