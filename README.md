# AstroTracker

Seguimiento y estabilización de vídeo astronómico para Windows.

Aplicación de escritorio **Open Source (GPLv3)** que sigue y mantiene centrados el
**Sol o la Luna** en vídeos grabados con cámaras DSLR/mirrorless sobre trípodes
manuales, sin star tracker.

Conceptualmente es un **"star tracker virtual"** aplicado a un vídeo ya grabado:
seleccionas el objeto, el tracker lo sigue a lo largo de la secuencia y el vídeo de
salida mantiene el objeto prácticamente inmóvil, aunque el usuario haya ido
moviendo manualmente la cámara.

Inspirado en [PIPP](https://astrowhat.com/resources/planetary-imaging-preprocessor-pipp.38),
pero resolviendo un problema que PIPP no cubre bien.

## Características clave

- **Nunca descarta frames.** Si el objeto se oculta (nube, montaña, edificio, fuera
  de encuadre), se conserva el frame y se predice la posición con el modelo de
  movimiento. Los frames no se pierden bajo ninguna circunstancia.
- **Sin plate solving ni astronomía.** No hace falta saber qué objeto es: se
  selecciona manualmente una ROI y se sigue.
- **Visual y sencillo.** Seleccionas el objeto, ves qué está haciendo el tracker y
  ajustas parámetros sobre un preview; no es un pipeline de 27 parámetros.
- **Tracking robusto**: TemplateTracker y CentroidTracker como núcleo, Kalman
  (x, y, vx, vy), estados VALID / UNCERTAIN / LOST, re-adquisición y predicción.
- **Estabilización por traslación** (XY) con centrado del objeto en un punto
  objetivo configurable.
- **Dos pasadas**: analizar/seguir y después aplicar/codificar, con preview de un
  rango de frames para experimentar rápido.
- **Visual debugging**: bounding box, centro detectado, trayectoria, confidence y
  estado de tracking superpuestos sobre el vídeo.
- **Modo "Fotos"**: abre una secuencia de fotos (JPG/PNG/TIFF/BMP; CR2/CR3 en
  camino vía LibRaw) en una pestaña con filmstrip de miniaturas y dos visores, y
  la centrará sobre el objeto foto a foto (en curso).

## Uso (flujo típico)

```text
Open video → Select object → Track → Preview → Stabilize → Export
```

1. Abrir un vídeo (MP4, MOV, AVI, SER).
2. Desplazarse a un frame donde el objeto sea claramente visible.
3. Dibujar un rectángulo alrededor del objeto (ROI).
4. Pulsar **Track**.
5. Ajustar parámetros (algoritmo, smoothing, predicción, centro) con preview.
6. Estabilizar y exportar (MP4, AVI, secuencia de imágenes, SER).

## Requisitos de build

| Componente | Versión | Notas |
| --- | --- | --- |
| Windows | 10/11 x64 | entorno principal |
| Visual Studio 2022 | Community o Build Tools | workload "Desktop development with C++" (MSVC + CMake + Ninja) |
| Qt | 6.8 LTS | prebuilt `msvc2022_64`, módulos Core, Gui, Widgets |
| OpenCV | 4.x | prebuilt de opencv.org |
| FFmpeg/libav | dev-package (gyan.dev) | build GPL con libx264 |
| vid.stab | 1.1.x (fuente) | terceros, módulo opcional de estabilización global |
| CMake | 3.21+ | incluido con VS |

La aplicación integra **FFmpeg como librería** (libavformat/libavcodec/libswscale);
no ejecuta `ffmpeg.exe` en producción.

## Compilar

```powershell
# Configurar (MSVC + VS generator, Release)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 -DOpenCV_DIR=C:/opencv/build

# Compilar
cmake --build build --config Release

# Tests (el ejecutable está en build/bin/Release con todas las DLLs)
ctest --test-dir build -C Release --output-on-failure
```

El build despliega automáticamente junto al ejecutable (`build/bin/Release`):
Qt (`windeployqt`), DLLs de FFmpeg (`bin/*.dll`) y `opencv_world*.dll`. No es
necesario añadir rutas al `PATH` para ejecutar la app.

Para abrir un vídeo al arrancar: `astrotracker.exe ruta\al\video.mp4`

Para generar el vídeo de prueba (disco brillante que se desplaza 4 px/frame):
`ffmpeg -f lavfi -i "color=c=black:s=640x480:r=25:d=10,geq=lum='if(lt((X-(80+mod(N*4,240)))*(X-(80+mod(N*4,240)))+(Y-240)*(Y-240),900),255,0)':cb=128:cr=128" -c:v libx264 -pix_fmt yuv420p testdata/moon.mp4`

## Estructura

```
src/
  app/          main, Application
  ui/           MainWindow (pestañas Vídeo|Fotos), VideoView, PhotoPanel, paneles
  video/        IVideoReader/IVideoWriter, FFmpegVideoReader/Writer, SERReader/Writer
  stills/       PhotoSequenceReader (secuencias de fotos; RAW próximamente)
  tracking/     ITracker, TemplateTracker, CentroidTracker, OpticalFlowTracker, HybridTracker
  motion/       MotionModel, Kalman, TrackStatus (VALID/UNCERTAIN/LOST)
  stabilization/ Stabilizer, TargetPosition, SmoothingFilter
  processing/   Pipeline, FrameTransformer, BorderHandler, Preprocessor
  export/       ExportJob
  common/       Frame, Rect2f, Logging
tests/          tests unitarios (CTest)
research/       scripts Python de prototipo/benchmark (no runtime de la app)
docs/           research.md, ARCHITECTURE.md, licencias
third_party/    dependencias fuente (ej. vid.stab)
resources/      iconos, estilos
```

## Documentación

- `docs/research.md` — investigación de proyectos Open Source (PIPP, Siril, SER
  Player, vid.stab, FFmpeg, OpenCV), licencias y decisiones.
- `THIRD_PARTY_LICENSES/` — licencias y obligaciones de cada dependencia.
- `changelog.md` — historial de cambios.
- `AGENTS.md` — guía para agentes de IA y decisiones técnicas fijadas.

## Licencia

**GPLv3** — véase `LICENSE`. El proyecto integra componentes con licencias
compatibles: FFmpeg (GPL), Qt (LGPLv3/GPLv3), OpenCV (Apache-2.0), vid.stab
(GPL-2.0-or-later), SER Player (MIT).

## Estado

En desarrollo (Fase 6). Completado: visor de vídeo, núcleo de tracking,
estabilización por traslación (Fase 3), pipeline de dos pasadas con exportación
FFmpeg (Fase 4), estabilización desde la UI (Fase 5) y el modo "Fotos" (Fase 6): 
pestaña separada con filmstrip de miniaturas, dos visores (original / centrado),
navegación y apertura por carpeta o multi-selección, con lectura de
JPG/PNG/TIFF/BMP y orden natural. Pendiente en Fase 6: lectura RAW
(CR2/CR3 vía LibRaw), seguimiento por círculo-máscara con predicción,
correcciones manuales arrastrando el círculo y exportación de la secuencia
centrada (fotos y/o MP4).

No está fuera del alcance actual: tratamiento de exposición/curvas tipo
darktable/lightroom (fase posterior).

No está en el MVP: plate solving, astronomía, IA, stacking, wavelets,
deconvolution ni calidad planetaria.