#!/usr/bin/env python3
"""Compara estrategias de correccion sobre mosaico Bayer con verdad conocida.

Escenas sinteticas que representan lo que hay que preservar:
  - rampa suave (gradiente atmosferico / senal debil)
  - borde escalon (limb lunar)
  - estrella gaussiana (PSF)
  - nebulosa con textura de baja frecuencia

En cada una se planta un hot pixel saturado y se corrige con:
  A  mediana de vecinos del mismo color
  B  media de vecinos del mismo color
  D  interpolacion direccional (elige el eje de menor gradiente)
  E  sin corregir (referencia del error)

Se mide el error introducido: RMS sobre la ventana del defecto, en DN.

Uso:
    python research/hotpixel_correction.py
"""

import numpy as np

H = W = 160
PATTERN = [[0, 1], [1, 2]]


def cfa_index(y, x):
    return PATTERN[y % 2][x % 2]


def vecinos_mismo_color(y, x, radio_sub=2):
    """Vecinos del MISMO color CFA que (y, x).

    radio_sub se mide en la sub-imagen de cada color (radio_sub=2 son 24
    vecinos, +-4 px en el mosaico), igual que en hotpixel_radius.py. Los
    offsets en el mosaico tienen que ser pares para conservar la paridad.
    """
    objetivo = cfa_index(y, x)
    out = []
    for dy in range(-2 * radio_sub, 2 * radio_sub + 1, 2):
        for dx in range(-2 * radio_sub, 2 * radio_sub + 1, 2):
            if dy or dx:
                if cfa_index(y + dy, x + dx) == objetivo:
                    out.append((y + dy, x + dx))
    return out


def ruido_sigma(m):
    """Sigma robusto global, para clavar la escala de valores."""
    med = np.median(m)
    return 1.4826 * np.median(np.abs(m - med))


def hotter(m, cy, cx, factor=8.0):
    med = np.median(m)
    sd = ruido_sigma(m)
    m[cy, cx] += factor * sd * max(sd, 1.0) * 0.25 + 6.0 * sd * 3.0
    return m


def corregir(estrategia, m, cy, cx, radio_sub=2):
    out = m.copy()
    vals = [m[y, x] for (y, x) in vecinos_mismo_color(cy, cx, radio_sub)]
    if estrategia == "A_mediana":
        out[cy, cx] = np.median(vals)
    elif estrategia == "B_media":
        out[cy, cx] = np.mean(vals)
    elif estrategia == "D_direccional":
        h = [m[cy, x] for x in (cx - 2, cx + 2)]
        v = [m[y, cx] for y in (cy - 2, cy + 2)]
        gh = abs(h[0] - h[1])
        gv = abs(v[0] - v[1])
        out[cy, cx] = np.mean(h if gh <= gv else v)
    return out


def escenas():
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    rnd = np.random.default_rng(5)

    rampa = 600 + 4.0 * xx + 2.0 * yy

    borde = np.where(xx < W / 2, 900.0, 3000.0)

    cx, cy = W / 2, H / 2
    estrella = 700 + 20000 * np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * 2.0 ** 2))

    nebulosa = (1200 + 600 * np.sin(xx / 9.0) * np.cos(yy / 7.0)
                + 300 * np.sin(xx / 3.0 + yy / 2.0))

    for nombre, base in (("rampa", rampa), ("borde", borde),
                         ("estrella", estrella), ("nebulosa", nebulosa)):
        ruido = rnd.normal(0, 8.0, base.shape)
        yield nombre, (base + ruido)


def main() -> int:
    print("%-10s %-14s %12s %12s" % ("escena", "estrategia", "RMS err", "max err"))
    for nombre, imagen in escenas():
        cy, cx = H // 2, W // 2
        verdad = imagen.copy()
        sucia = hotter(imagen.copy(), cy, cx)

        # referencia: que error hay ya sin corregir (por el hot pixel)
        if nombre != "estrella":
            print("%-10s %-14s %12.2f %12.2f"
                  % (nombre, "sin corregir",
                     np.sqrt(np.mean((sucia[cy - 2:cy + 3, cx - 2:cx + 3]
                                      - verdad[cy - 2:cy + 3, cx - 2:cx + 3]) ** 2)),
                     np.abs(sucia[cy - 2:cy + 3, cx - 2:cx + 3]
                            - verdad[cy - 2:cy + 3, cx - 2:cx + 3]).max()))

        for est in ("A_mediana", "B_media", "D_direccional"):
            out = corregir(est, sucia, cy, cx)
            d = out[cy - 2:cy + 3, cx - 2:cx + 3] - verdad[cy - 2:cy + 3, cx - 2:cx + 3]
            print("%-10s %-14s %12.2f %12.2f"
                  % (nombre, est, np.sqrt(np.mean(d ** 2)), np.abs(d).max()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())