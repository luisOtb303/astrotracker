#!/usr/bin/env python3
"""Sonda para validar el detector de pixeles calientes contra fotos reales.

No forma parte de la app: es la herramienta con la que se calibro el umbral.
Reproduce exactamente el algoritmo de img::detectHotPixels() para poder
comprobar si los pixeles que el usuario ve a ojo salen como defecto y cuantos
falsos positivos genera el resto de la imagen.

Uso:
    python research/hotpixel_probe.py testdata/hotpixel/IMG_5633.jpg 1102,926 ...
"""

import sys

import cv2
import numpy as np

SIGMA_MIN = 3.0
SIGMA_MAX = 30.0


def sensitivity_to_sigma(v: int) -> float:
    t = min(max(v, 0), 100) / 100.0
    return SIGMA_MIN + t * (SIGMA_MAX - SIGMA_MIN)


def local_median(frame: np.ndarray) -> np.ndarray:
    return cv2.medianBlur(frame, 3)


def noise_sigma(frame: np.ndarray, med: np.ndarray) -> float:
    residual = cv2.absdiff(frame, med)
    # std de |a-b| = raiz(2) * std del ruido
    return float(np.mean(np.std(residual, axis=2))) / np.sqrt(2.0)


def detect(frame: np.ndarray, sensitivity: int, dilate: int = 1):
    med = local_median(frame)
    sigma = noise_sigma(frame, med)
    k = sensitivity_to_sigma(sensitivity)
    thr = max(0.2, k * sigma)

    residual = cv2.absdiff(frame, med).astype(np.float32)
    cand = (residual > thr).any(axis=2).astype(np.uint8) * 255

    # Solo los candidatos realmente solos: el vecindario excluye el centro.
    kernel = np.ones((3, 3), np.uint8)
    kernel[1, 1] = 0
    neighbours = cv2.dilate(cand, kernel)
    isolated = cv2.bitwise_and(cand, cv2.bitwise_not(neighbours))

    hot_count = int(np.count_nonzero(isolated))
    mask = isolated
    if hot_count > 0 and dilate > 0:
        k2 = cv2.getStructuringElement(
            cv2.MORPH_RECT, (2 * dilate + 1, 2 * dilate + 1))
        mask = cv2.dilate(isolated, k2)
    return mask, isolated, thr, sigma


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    path = sys.argv[1]
    points = []
    for arg in sys.argv[2:]:
        x, y = (int(v) for v in arg.split(","))
        points.append((x, y))

    bgr = cv2.imread(path, cv2.IMREAD_COLOR)
    if bgr is None:
        print(f"no se pudo leer {path}")
        return 1
    h, w = bgr.shape[:2]
    print(f"{path}: {w}x{h}")

    for sens in (20, 35, 50, 65, 80):
        mask, raw, thr, sigma = detect(bgr, sens, dilate=0)
        hits = []
        for (x, y) in points:
            val = int(bgr[y, x].max())
            delta = float(np.abs(bgr[y, x].astype(np.float32) -
                                 cv2.medianBlur(bgr[y:y + 1, x:x + 1], 3)[0, 0]).max())
            ok = raw[y, x] != 0
            hits.append((x, y, val, delta, ok))
        total = int(np.count_nonzero(raw))
        print(f"\nsens={sens:3d}  k={sensitivity_to_sigma(sens):5.1f} sigma  "
              f"thr={thr:7.2f}  detecciones={total}")
        for (x, y, val, delta, ok) in hits:
            print(f"   ({x:5d},{y:5d})  valor={val:3d}  delta={delta:7.1f}  "
                  f"{'DETECTADO' if ok else '-'}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())