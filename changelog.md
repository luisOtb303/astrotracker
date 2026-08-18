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