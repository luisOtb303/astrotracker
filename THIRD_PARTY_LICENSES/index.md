# Third-party licenses

AstroTracker es **GPLv3**. Las dependencias integradas son prebuilt (binarios
linkeados/copiados) o fuentes propias; cada componente mantiene su propia
licencia. Las obligaciones de distribución (copyleft, avisos, textos de licencia)
se documentan aquí.

| Componente | Licencia | Tipo | Uso en AstroTracker |
| --- | --- | --- | --- |
| Qt 6 | LGPLv3 (GPLv3 también permitido) | prebuilt | UI Widgets (dinámico) |
| OpenCV | Apache-2.0 | prebuilt | visión: `cv::Mat`, tracking |
| FFmpeg (gyan.dev full build) | LGPLv2.1+ **con libx264 → GPLv2/v3** | prebuilt | decodificación/codificación vídeo |
| vid.stab | GPL-2.0-or-later | fuente (`third_party/`) | módulo opcional, fuera del MVP |
| LibRaw | **LGPL-2.1** + opciones CDDL-1.0 | fuente (`third_party/libraw/`, estática) | decodificación RAW (CR2/CR3, DNG, NEF…) |
| exiv2 | GPL-2.0-or-later | fuente (`third_party/exiv2/`, estática) | lectura EXIF de JPEG/PNG/TIFF |
| SER Player | MIT | referencia (docs) | protocolo SER v3 |
| opencv_contrib | Apache-2.0 | (no usado aún) | trackers CSRT/KCF (opcional futuro) |

## Obligaciones

- **FFmpeg**: la distribución debe incluir el texto de la licencia GPL (o LGPL)
  y avisos de copyright. Los binarios de gyan.dev se redistribuyen solo en el
  paquete de la app, sin modificar.
- **Qt (LGPLv3)**: uso dinámico (`windeployqt` copia las DLLs); no se modifica.
  Si se llegara a modificar Qt, se aplicaría la sección 4 de la LGPLv3.
- **vid.stab**: si se distribuye compilado, hay que incluir la fuente o la oferta
  de fuente (GPL-2.0-or-later) y el texto de licencia en `third_party/vid.stab`.
- **LibRaw (LGPL-2.1/CDDL-1.0)**: al distribuir la app hay que (a) conservar los
  textos `COPYRIGHT`, `LICENSE.LGPL` y `LICENSE.CDDL` junto a
  `third_party/libraw/`, y (b) ofrecer la fuente de LibRaw tal y como exige la
  LGPL-2.1 sección 6 (se cumple porque se distribuye el árbol fuente completo
  en-tree). No se modifica LibRaw.
- **exiv2 (GPL-2.0-or-later)**: al distribuir la app hay que (a) conservar
  `COPYING` y `LICENSE.txt` junto a `third_party/exiv2/`, y (b) ofrecer la
  fuente de exiv2 (se cumple porque se distribuye el árbol fuente completo
  en-tree). El texto GPL-2.0 se incluye en `licenses/` del paquete.

## Textos de licencia

- Los textos completos de cada licencia deben incluirse en el paquete de
  distribución de la app (carpeta `licenses/` del instalador/zip).
- Estado: pendiente de recopilar los textos exactos de gyan.dev, Qt, OpenCV y
  vid.stab en la fase de empaquetado.
