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

### Fixed

- `FFmpegVideoReader::convertFrame`: el constructor de `cv::Mat` recibía
  `(width, height)` en orden inverso (rows/cols), produciendo frames
  transpuestos (480x640 en lugar de 640x480).