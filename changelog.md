# Changelog

Todas las modificaciones notables de AstroTracker se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es/1.1.0/) y el
versionado es [SemVer](https://semver.org/lang/es/) (MAJOR.MINOR.PATCH).

## [Unreleased]

### Added

- Inicialización del repositorio: `AGENTS.md` (decisiones técnicas y comandos de
  build/test), `README.md`, `changelog.md`, `.gitignore`, CMakeLists raíz.
- `docs/research.md`: investigación (PIPP, vid.stab, OpenCV, FFmpeg, SER Player,
  Siril) y decisiones de arquitectura.
- `THIRD_PARTY_LICENSES/index.md`: inventario de licencias y obligaciones.
- Fase 1 — Visor de vídeo operativo:
  - `FFmpegVideoReader` (libav) funcional: decodifica MP4/H.264 a `cv::Mat` BGR8 vía
    `sws_scale`, seek por PTS, duración/fps/frameCount, corrección de dimensiones
    rows/cols.
  - `VideoView`: pinta el frame con `QImage` y selección de ROI por arrastre (señal
    `roiSelected`).
  - `MainWindow`: abrir vídeo (diálogo o argumento CLI), reproducir/pausar,
    frame a frame, detener, slider por milisegundos, contador de frame y timestamp.
  - `main.cpp`: arranca la app y opcionalmente abre el vídeo pasado por argumento.
  - `FindFFmpeg.cmake`: módulo propio (se resuelve ANTES de Qt para que Qt no lo
    sombree) con targets importados `FFmpeg::avformat|avcodec|avutil|swscale`.
  - Deploy automático de DLLs (Qt/FFmpeg/OpenCV) junto al ejecutable en
    `build/bin/Release`.
  - `tests/smoke_reader`: smoke test de decodificación/seek del reader (CTest).
  - `testdata/moon.mp4`: vídeo sintético de prueba (disco brillante móvil).

### Added

- Fase 3 — Estabilización por traslación:
  - `TargetPosition`: punto objetivo (por defecto, centro del frame) donde se
    mantiene el objeto.
  - `SmoothingFilter`: EMA sobre el centro del objeto para atenuar el jitter.
  - `Stabilizer`: calcula el desplazamiento (dx, dy) que centra el objeto
    (`warpAffine` con bordes negros) y expone `target`/`offset`.
  - `tests/test_stabilizer` (CTest): convergencia del offset, centrado del
    objeto tras `apply` y atenuación del jitter.

### Fixed

- `FFmpegVideoReader::convertFrame`: el constructor de `cv::Mat` recibía
  `(width, height)` en orden inverso (rows/cols), produciendo frames
  transpuestos (480x640 en lugar de 640x480).
- `Kalman::correct`: la ganancia se calculaba con matrices 4×4 singulares
  (la medición es 2D); ahora se usa matemática 2×2 y el filtro converge
  correctamente a posición y velocidad.

### Added

- Fase 2 — Núcleo de tracking:
  - `ITracker`: interfaz de trackers (init + track), `TrackResult` con
    confianza y flag `found`.
  - `TemplateTracker`: `matchTemplate` (TM_CCOEFF_NORMED) en ventana de
    búsqueda alrededor de la última posición.
  - `CentroidTracker`: umbral OTSU + componente conexo más grande + centroide;
    tolerante a oclusión parcial.
  - `motion/Kalman`: filtro de velocidad constante (x, y, vx, vy) con
    `predict()`/`correct()`.
  - `motion/MotionModel`: integra mediciones + predicción y estados
    `VALID / UNCERTAIN / LOST` con `consecutiveMisses`.
  - `tests/test_motion` y `tests/test_trackers` (CTest): convergencia de
    Kalman, transiciones de estado y seguimiento sobre frames sintéticos.

### Added

- Fase 4 — Pipeline de dos pasadas y exportación:
  - `IVideoWriter`: interfaz de escritura de vídeo (open/write/close).
  - `FFmpegVideoWriter` (libav/libx264): codifica BGR8 → H.264/MP4 con PTS
    monotónicos por índice de paquete y duración explícita por frame
    (`pkt->duration = 1`), sin B-frames y sin threading para latencia mínima.
  - `BorderHandler`: aplicación de `warpAffine` con borde negro o réplica.
  - `FrameTransformer`: traslación de un frame respetando el modo de borde.
  - `Pipeline::analyze` (primera pasada): seguimiento + suavizado + offsets.
  - `Pipeline::run` (segunda pasada): re-lectura del vídeo de entrada,
    transformación frame a frame y escritura de la salida, sin descartar
    ningún frame.
  - `ExportJob` (esqueleto): tarea de exportación para el hilo de UI.
  - `tests/test_writer` (CTest): 30 frames ida y vuelta (writer → reader) con
    contenido verificado.
  - `tests/test_pipeline` (CTest): vídeo sintético de 40 frames, tracking
    válido en todos, salida con 40 frames y objeto centrado (≤5 px).

### Fixed

- `FFmpegVideoWriter`: el último frame se perdía (30 paquetes en el contenedor,
  solo 29 decodificables): los paquetes salían con `duration = 0` y el muxer MP4
  escribía la duración del track igual al último PTS, marcando la última muestra
  como descartable. Se fija `pkt->duration = 1` en cada paquete emitido.
- `FFmpegVideoWriter`: los frames encolados se liberaban antes de que el encoder
  terminara de consumirlos; ahora se conservan en una cola FIFO hasta que se
  emite el paquete correspondiente.
- `FFmpegVideoReader`: se fuerza decodificación mono-hilo (`thread_count = 1`)
  para evitar que el frame-threading devuelva contenido obsoleto.
- `tests/test_writer` y `tests/test_pipeline`: los centroides se calculaban
  sobre toda la imagen (el fondo gris de 20/255 sesgaba el resultado hacia el
  centro del frame); ahora se umbraliza antes de `cv::moments`.
- `tests/test_pipeline`: la ROI de prueba no estaba centrada en el disco y la
  velocidad (9,4 px/frame) producía un lag de suavizado que excedía la
  tolerancia; se centra la ROI en el disco y se usa una deriva lenta realista.

### Added

- Fase 5 — Estabilización desde la UI:
  - `PipelineSettings`: parámetros del pipeline (tracker Template/Centroid,
    searchFactor, alpha de suavizado, modo de borde y punto objetivo).
  - `Pipeline::analyze`/`run` parametrizados con `PipelineSettings`, callback
    de progreso y `startUs` (permite seguir desde el frame donde se seleccionó
    la ROI; los frames previos pasan sin estabilizar).
  - `Stabilizer::setSmoothing`: alpha configurable del suavizado.
  - `ExportJob` con `PipelineSettings` y progreso.
  - `PipelineWorker` (QThread): ejecuta la pasada 1 (analizar) o la 1+2
    (exportar) fuera del hilo de UI, emitiendo progreso y resultado por señales.
  - `MainWindow`: dos visores lado a lado (Original / Estabilizado) que avanzan
    sincronizados; toolbar de estabilización (tracker, borde, suavizado) con
    botones **Seguir**, **Vista previa** y **Exportar...**; barra de progreso en
    la barra de estado; la ROI seleccionada queda dibujada de forma persistente
    y el análisis parte del frame en que se eligió.
  - `VideoView::setRoi`/`clearRoi`/`setRoiEnabled`: overlay de ROI persistente.
  - `testdata/moon.mp4` regenerado: el disco ahora se desplaza de verdad
    (el `drawbox` con variable `t` no animaba en este build de ffmpeg; se usa
    `geq` con `N`).

### Fixed

- `Pipeline::run`: se alinean los offsets con `startUs` (los frames anteriores
  al inicio del seguimiento se conservan sin transformar, sin descartar nada).
- `tests/test_pipeline`: se añade `CentroidTracker.cpp` al target (el pipeline
  ahora lo instancia).