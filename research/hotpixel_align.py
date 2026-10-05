#!/usr/bin/env python3
"""Alinea el mosaico RAW con el JPEG de camara para poder comparar coords.

raw_image_visible (5202x3464) y el JPEG (5184x3456) no tienen el mismo tamano:
la camara recorta un borde de pixels enmascarados. Sin conocer el desplazamiento
no se puede Transladar lo que el usuario ve en el JPEG a coordenadas del RAW, que
es donde habria que corregir.

Usa phase correlation sobre versiones reducidas (el contenido es el mismo aunque
uno este en mosaico y el otro demosaicado con gamma) y luego refina con la
posicion de los outliers mas fuertes.

Uso:
    python research/hotpixel_align.py imagen.CR2 imagen.jpg 1102,926 ...
"""

import sys

import cv2
import numpy as np
import rawpy


def load_raw_gray(path, max_dim=600):
    with rawpy.imread(path) as raw:
        a = raw.raw_image_visible.astype(np.float32)
    a = a / max(a.max(), 1.0)
    bgr = cv2.cvtColor(a, cv2.COLOR_GRAY2BGR)
    scale = max_dim / max(bgr.shape[0], bgr.shape[1])
    return cv2.resize(bgr, None, fx=scale, fy=scale,
                      interpolation=cv2.INTER_AREA), scale


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    raw_path, jpg_path = sys.argv[1], sys.argv[2]
    points = [tuple(int(v) for v in a.split(",")) for a in sys.argv[3:]]

    raw_small, scale = load_raw_gray(raw_path)
    jpg = cv2.imread(jpg_path, cv2.IMREAD_COLOR)
    jpg_gray = cv2.cvtColor(jpg, cv2.COLOR_BGR2GRAY).astype(np.float32)
    jpg_gray = jpg_gray / max(jpg_gray.max(), 1.0)

    print(f"RAW reducido: {raw_small.shape[1]}x{raw_small.shape[0]} (escala {scale:.4f})")
    print(f"JPEG:         {jpg.shape[1]}x{jpg.shape[0]}")

    # phaseCorrelate exige single-channel y del mismo tipo.
    raw_g = np.ascontiguousarray(raw_small[:, :, 0], dtype=np.float32)
    jpg_small = cv2.resize(jpg_gray, (raw_small.shape[1], raw_small.shape[0]),
                           interpolation=cv2.INTER_AREA)
    jpg_g = np.ascontiguousarray(jpg_small, dtype=np.float32)

    win = cv2.createHanningWindow((raw_g.shape[1], raw_g.shape[0]), cv2.CV_32F)
    (dx, dy), resp = cv2.phaseCorrelate(raw_g, jpg_g, win)
    ox, oy = dx / scale, dy / scale
    print(f"desplazamiento estimado (RAW = JPEG + ({ox:.1f},{oy:.1f}))  "
          f"respuesta={resp:.3f}")

    print("\ncoordenadas del usuario traducidas al RAW:")
    with rawpy.imread(raw_path) as raw:
        mosaic = raw.raw_image_visible.astype(np.float32)
        pattern = np.array(raw.raw_pattern)
    h, w = mosaic.shape
    for (x, y) in points:
        rx, ry = int(round(x + ox)), int(round(y + oy))
        if not (0 <= rx < w and 0 <= ry < h):
            print(f"  ({x},{y}) -> RAW ({rx},{ry})  FUERA DE RANGO")
            continue
        cfa = pattern[ry % 2, rx % 2]
        print(f"  ({x},{y}) -> RAW ({rx},{ry})  cfa={cfa}  valor={mosaic[ry, rx]:.0f}")
        med3 = cv2.medianBlur(cv2.cvtColor(
            np.clip(mosaic[ry - 2:ry + 3, rx - 2:rx + 3], 0, 65535).astype(np.uint16),
            cv2.COLOR_GRAY2BGR), 3)
        print(f"      mediana 5x5={float(np.median(mosaic[ry-2:ry+3, rx-2:rx+3])):.0f}"
              f"   max 5x5={float(mosaic[ry-2:ry+3, rx-2:rx+3].max()):.0f}"
              f"   (mediana3={float(med3[2, 2][0]):.0f})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())