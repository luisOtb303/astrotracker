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

## Textos de licencia

- Los textos completos de cada licencia deben incluirse en el paquete de
  distribución de la app (carpeta `licenses/` del instalador/zip).
- Estado: pendiente de recopilar los textos exactos de gyan.dev, Qt, OpenCV y
  vid.stab en la fase de empaquetado.
