#!/usr/bin/env python3
"""Compara familias de deteccion sobre el mosaico RAW, con puntos de referencia.

Positivos conocidos: las coordenadas que indico el usuario en el JPEG,不定
trasladas al mosaico con el desplazamiento medido (10, 4). Cada positivo se
localiza como el maximo del residual dentro de +-3 px, para no depender de la
precision con la que el usuario leyo la coordenada.

Se reportan, por familia y por umbral:
  - cuantos positivos se detectan (sensibilidad);
  - cuantos falsos positivos salen (especificidad);
  - que ocurre con una estructura real de baja frecuencia.

Uso:
    python research/hotpixel_eval.py imagen.CR2 dx,dy px,py px,py ...
"""

import sys

import numpy as np
import rawpy

# desplazamiento medido empirically entre JPEG de camara y raw_image_visible
OFFSET = (10, 4)
BUSQUEDA = 3          # radio para localizar el positivo real
RADIO = 2             # radio del vecindario en la sub-imagen de cada color
                       # (24 vecinos; ver hotpixel_radius.py)


def load(path):
    with rawpy.imread(path) as raw:
        return raw.raw_image_visible.astype(np.float32), np.array(raw.raw_pattern)


def split_paridad(mosaic):
    return [mosaic[py::2, px::2].copy() for py in range(2) for px in range(2)]


def vecindarios(sub, radio):
    """Apila vecinos del mismo color (ya separado por paridad): (K,h,w)."""
    pad = np.pad(sub, radio, mode="reflect")
    h, w = sub.shape
    out = []
    for dy in range(-radio, radio + 1):
        for dx in range(-radio, radio + 1):
            if dy or dx:
                out.append(pad[radio + dy:radius_safe(dy, radio, h),
                               radius_safe(dx, radio, w)])
    return out


def radius_safe(v, radio, size):
    return min(max(v, 0), size - 1)


def stats(views, centre):
    st = np.stack(views, axis=0)
    med = np.median(st, axis=0)
    mad = np.median(np.abs(st - med), axis=0)
    std = st.std(axis=0)
    return med, mad, std, st


def run_family(mosaic, pattern, radio=RADIO):
    """Calcula los campos de cada familia sobre las 4 paridades."""
    subs = split_paridad(mosaic)
    campos = []
    for i in range(4):
        sub = subs[i]
        views = []
        pad = np.pad(sub, radio, mode="reflect")
        h, w = sub.shape
        for dy in range(-radio, radio + 1):
            for dx in range(-radio, radio + 1):
                if dy or dx:
                    views.append(pad[radio + dy:radio + dy + h,
                                     radio + dx:radio + dx + w])
        campos.append((sub, views))
    return campos


def eval_family(campos, k_sigma, use_local=True, use_isolation=True):
    """Devuelve (mapa por paridad de z, detecciones, candidatos)."""
    det = []
    cand_tot = 0
    for (sub, views) in campos:
        st = np.stack(views, axis=0)
        med = np.median(st, axis=0)
        mad = np.median(np.abs(st - med), axis=0)
        std = st.std(axis=0)

        if use_local:
            sigma = 1.4826 * mad
        else:
            sigma = np.full_like(mad, np.std(mosaic_global)) if False else None

        sigma = np.maximum(sigma if use_local else std, k_sigma * 0.5)
        z = (sub - med) / sigma
        cand = z > k_sigma
        cand_tot += int(cand.sum())
        if use_isolation:
            nb_max = st.max(axis=0)
            cand = cand & (nb_max <= med + k_sigma * sigma)
        det.append(cand)
    return det, cand_tot


def to_full(det_par, h, w):
    full = np.zeros((h, w), bool)
    for i, (py, px) in enumerate([(0, 0), (0, 1), (1, 0), (1, 1)]):
        full[py::2, px::2] = det_par[i]
    return full


def find_positive(mosaic, med_full, x, y, search=BUSQUEDA):
    """Exceso maximo en un vecindario: donde esta de verdad el defecto."""
    h, w = mosaic.shape
    best = None
    for dy in range(-search, search + 1):
        for dx in range(-search, search + 1):
            yy, xx = y + dy, x + dx
            if 0 <= yy < h and 0 <= xx < w:
                exc = mosaic[yy, xx] - med_full[yy, xx]
                if best is None or exc > best[0]:
                    best = (float(exc), xx, yy)
    return best


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    path = sys.argv[1]
    offx, offy = (int(v) for v in sys.argv[2].split(","))
    pts = [tuple(int(v) for v in a.split(",")) for a in sys.argv[3:]]

    mosaic, pattern = load(path)
    h, w = mosaic.shape
    megapix = (h * w) / 1e6
    print(f"{path}: {w}x{h} ({megapix:.1f} Mpx)  patron={pattern.tolist()}")

    # Referencia global de ruido para las familias no robustas.
    pad = np.pad(mosaic, RADIO, mode="reflect")
    h2, w2 = mosaic.shape
    med_all = np.empty_like(mosaic)
    stack = np.stack([pad[RADIO + dy:RADIO + dy + h2,
                          RADIO + dx:RADIO + dx + w2]
                      for dy in range(-RADIO, RADIO + 1)
                      for dx in range(-RADIO, RADIO + 1)], axis=0)
    med_all = np.median(stack, axis=0)
    del stack

    # Localizar los positivos declarados.
    positivos = []
    for (jx, jy) in pts:
        x, y = jx + offx, jy + offy
        exc, bx, by = find_positive(mosaic, med_all, x, y)
        z = exc / max(1.4826 * np.median(
            np.abs(np.stack([pad[RADIO + dy:RADIO + dy + h2,
                              RADIO + dx:RADIO + dx + w2]
                             for dy in range(-RADIO, RADIO + 1)
                             for dx in range(-RADIO, RADIO + 1)
                             if dy or dx], axis=0) - med_all), axis=0)[by, bx], 1.0)
        positivos.append((bx, by))
        print(f"  positivo declarado JPEG({jx},{jy}) -> RAW({bx},{by}) "
              f"exceso={exc:+8.0f}  z_local={z:7.1f}")

    campos = run_family(mosaic, pattern)

    print("\n%-42s %8s %10s %8s" % ("familia", "umbral", "detectados", "FP/Mpx"))
    for nombre, k, local, iso in [
            ("mediana local + std global", 6, False, False),
            ("mediana local + std local", 6, False, True),
            ("mediana local + MAD local", 5, True, False),
            ("mediana local + MAD local", 8, True, False),
            ("mediana local + MAD local + aislamiento", 8, True, True),
            ("mediana local + MAD local + aislamiento", 12, True, True),
            ("mediana local + MAD local + aislamiento", 20, True, True),
    ]:
        det, cand = eval_family(campos, k, local, iso)
        full = to_full(det, h, w)
        hits = sum(1 for (x, y) in positivos if full[y, x])
        n = int(full.sum())
        print("%-42s %8s %10d %8.1f   (positivos %d/%d, candidatos %d)"
              % (nombre, k, n, n / megapix, hits, len(positivos), cand))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())