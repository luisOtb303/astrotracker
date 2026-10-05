#!/usr/bin/env python3
"""Barrido de sigmaFloor para el filtro de pixeles calientes.

Replica exactamente el detector de src/raw/BayerHotPixels.cpp:

  1. separa el mosaico en las 4 paridades (y&1, x&1);
  2. por cada paridad saca los 4x4 vecinos del mismo color (radio 2) y calcula
     la mediana y el MAD LOCALES con ventanas 5x5 que INCLUYEN el centro;
  3. umbral  thr = med + k * max(1.4826 * MAD, sigmaFloor);
  4. marca  sub > thr  y ademas exige aislamiento (ningun vecino del mismo
     color supera el umbral).

La pregunta que responde: al subir sigmaFloor de 1 a 4 DN, ¿se pierden los
pixeles quemados de verdad o solo se van los falsos positivos del ruido?

Salida por cada sigmaFloor: cuantos candidatos detuve, y de esos cuantos
coinciden con los candidatos "fuertes" del barrido de referencia. Ademas
imprime cuanto aprieta el suelo: que porcentaje de pixeles tiene MAD tan bajo
que el suelo manda, que es donde nacen los falsos positivos.

Uso:
    python research/hotpixel_sigmafloor.py imagen.CR2 [otra.CR2 ...]
"""

import sys

import numpy as np
import rawpy

RADIO = 2          # mismo que HotPixelParams::radius por defecto
KSIGMA = 20.0      # mismo que el default del slider (sensibilidad 50)
FLOORS = (1.0, 2.0, 4.0, 8.0, 16.0)
FILAS = 192        # bloques para no pedir 900 MB a numpy de golpe


def cargar(path):
    with rawpy.imread(path) as raw:
        mosaico = raw.raw_image_visible.astype(np.int32)
    return mosaico





def evaluar_simple(sub, sigma_floor, k=KSIGMA):
    """Version en una pasada: sin bloques, asumiendo sub pequena."""
    r = RADIO
    pad = np.pad(sub, r, mode="reflect")
    alto, ancho = sub.shape
    # pad ya lleva r celdas de margen a cada lado, asi que el desplazamiento de
    # la ventana es dy, dx directamente (no r+dy: el pad ya esta desplazado).
    ventana = np.stack([pad[dy:dy + alto, dx:dx + ancho]
                        for dy in range(2 * r + 1) for dx in range(2 * r + 1)], 0)
    med = np.median(ventana, axis=0)
    mad = np.median(np.abs(ventana - med), axis=0)
    thr = med + k * np.maximum(1.4826 * mad, sigma_floor)
    cand = sub > thr
    nbr = ventana.copy()
    nbr[12] = -1  # el centro es el indice 12 de la ventana 5x5
    det = cand & (nbr.max(axis=0) <= thr)
    # cuanto aprieta el suelo
    atado = (1.4826 * mad) < sigma_floor
    return det, int(cand.sum()), int(atado.sum()), int(atado.size)


def barrido(mosaico):
    par = [mosaico[py::2, px::2].astype(np.int32) for py in range(2)
           for px in range(2)]
    print("  paridad    detect   cand_sin_aislar   suelo_manda")
    ref = {}
    for floor in FLOORS:
        tot = cand = at = size = 0
        mapa = np.zeros(mosaico.shape, bool)
        for i, sub in enumerate(par):
            d, c, a, s = evaluar_simple(sub, floor)
            mapa[i // 2::2, i % 2::2] = d
            tot += int(d.sum())
            cand += c
            at += a
            size += s
        ref[floor] = mapa
        print("  floor=%-5.1f %7d %13d %11.2f%%" %
              (floor, tot, cand, 100.0 * at / size))
    return ref


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    for path in argv[1:]:
        print("[archivo] " + path)
        mosaico = cargar(path)
        ref = barrido(mosaico)
        fuerte = ref[FLOORS[0]]
        for floor in FLOORS[1:]:
            m = ref[floor]
            comun = int((m & fuerte).sum())
            print("  floor=%-5.1f conserva %d/%d de los de floor=%.1f" %
                  (floor, comun, int(fuerte.sum()), FLOORS[0]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))