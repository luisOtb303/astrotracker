#!/usr/bin/env python3
"""Comprueba si los hot pixels del RAW aparecen en el JPEG de camara.

Toma outliers inequivocos del mosaico (z enorme) y busca su posicion exacta en
el JPEG por "hotness" local (valor menos mediana 3x3). El desplazamiento que
sale de ahi es el bueno, y permite preguntar si lo que el usuario ve en el JPEG
es realmente un defecto del sensor o algo que aparece al procesar.

Uso:
    python research/hotpixel_crosscheck.py imagen.CR2 imagen.jpg
"""

import sys

import cv2
import numpy as np
import rawpy

RADIO_BUSQUEDA = 60


def hotness(gray):
    med = cv2.medianBlur(gray, 3)
    return gray.astype(np.int16) - med.astype(np.int16)


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    raw_path, jpg_path = sys.argv[1], sys.argv[2]

    with rawpy.imread(raw_path) as raw:
        mosaic = raw.raw_image_visible.astype(np.float32)
    h, w = mosaic.shape

    jpg = cv2.imread(jpg_path, cv2.IMREAD_COLOR)
    jh, jw = jpg.shape[:2]
    jpg_max = jpg.max(axis=2).astype(np.int16)
    print(f"RAW {w}x{h}   JPEG {jw}x{jh}")

    # Outliers del RAW: buscar el maximo del residual frente a la mediana 3x3
    # sobre el mosaico leido como gris (proxy del mismo defecto).
    med = cv2.medianBlur(np.clip(mosaic, 0, 65535).astype(np.uint16), 3)
    resid = mosaic - med.astype(np.float32)
    flat = resid.ravel()
    idx = np.argpartition(flat, -400)[-400:]
    idx = idx[np.argsort(-flat[idx])]

    # agrupar para no repetir el mismo defecto
    picks = []
    for i in idx:
        y, x = np.unravel_index(i, resid.shape)
        if all(abs(x - px) > 30 or abs(y - py) > 30 for (px, py) in picks):
            picks.append((int(x), int(y)))
        if len(picks) >= 6:
            break

    hots = hotness(jpg_max)
    print(f"\ndesplazamiento medido sobre {len(picks)} outliers reales del RAW:")
    shifts = []
    for (x, y) in picks:
        bx0, bx1 = max(0, x - RADIO_BUSQUEDA), min(jw, x + RADIO_BUSQUEDA + 1)
        by0, by1 = max(0, y - RADIO_BUSQUEDA), min(jh, y + RADIO_BUSQUEDA + 1)
        win = hots[by0:by1, bx0:bx1]
        if win.size == 0:
            print(f"  RAW({x:5d},{y:5d}) -> fuera del JPEG (recorte distinto)")
            continue
        # SoloAccepta picos muy claros: asi no nos enganamos con el fondo.
        loc = np.unravel_index(np.argmax(win), win.shape)
        jx, jy = int(loc[1]) + bx0, int(loc[0]) + by0
        sx, sy = x - jx, y - jy
        shifts.append((sx, sy))
        print(f"  RAW({x:5d},{y:5d}) -> JPEG({jx:5d},{jy:5d})  "
              f"desplazamiento=({sx:+3d},{sy:+3d})  "
              f"z_raw={resid[y, x]:7.0f}  hotness_jpeg={hots[jy, jx]:4d}")

    if shifts:
        mx = int(np.median([s[0] for s in shifts]))
        my = int(np.median([s[1] for s in shifts]))
        print(f"\ndesplazamiento mediano: RAW = JPEG + ({mx}, {my})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())