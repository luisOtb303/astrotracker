# AGENTS.md

Guía para agentes de IA que trabajen en este repositorio.

## Proyecto

**AstroTracker** — aplicación de escritorio para Windows (C++ / Qt 6 / CMake / MSVC 2022)
que sigue y estabiliza el Sol o la Luna en vídeos grabados con DSLR/mirrorless sobre
trípodes manuales (sin star tracker).

Concepto: "star tracker virtual" aplicado a un vídeo ya grabado. No hace plate solving,
no necesita conocer el objeto, **nunca descarta frames** aunque el tracking se pierda.

Véase la especificación completa en `docs/` (research.md y futuros docs).

## Decisiones técnicas (fijadas, no cambiar sin justificación)

- Licencia del proyecto: **GPLv3**
- UI: **Qt 6 Widgets** (no QML)
- Estándar: **C++17**
- Build: **CMake** (MSVC 2022, también compilable desde línea de comandos)
- Video: **FFmpeg/libav** integrado como librería (NO ejecutar ffmpeg.exe en producción)
  mediante las interfaces `IVideoReader` / `IVideoWriter`
- Imágenes/visión: **OpenCV** (`cv::Mat`)
- vid.stab: referencia para el subsistema de estabilización y módulo opcional de
  "global stabilization"; el núcleo es object-tracking propio
- Trackers: TemplateTracker y CentroidTracker son los principales. CSRT es opcional
  (requiere opencv_contrib) y NO se asume como solución definitiva
- Pipeline: **dos pasadas** — (1) analizar/seguir/calcular transforms, (2) aplicar y codificar

## Commands

Entorno objetivo: Windows, Visual Studio 2022 Build Tools, CMake, Ninja.

- Configurar build (Release, MSVC + Ninja):
  `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
  o con Ninja:
  `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
- Compilar:
  `cmake --build build --config Release`
- Tests (CTest):
  `ctest --test-dir build -C Release --output-on-failure`
- Lint/format: no hay lint configurado todavía; seguir el estilo del código vecino.

## Estructura

```
src/
  app/          main, Application
  ui/           MainWindow (pestañas Vídeo|Fotos), VideoView, PhotoPanel, paneles
  video/        IVideoReader/IVideoWriter, FFmpegVideoReader/Writer, SERReader/Writer
  stills/       PhotoSequenceReader (secuencias de fotos; RAW con LibRaw en curso)
  tracking/     ITracker, TemplateTracker, CentroidTracker, OpticalFlowTracker, HybridTracker
  motion/       MotionModel, Kalman, TrackStatus (VALID/UNCERTAIN/LOST)
  stabilization/ Stabilizer, TargetPosition, SmoothingFilter
  processing/   Pipeline, FrameTransformer, BorderHandler, Preprocessor
  export/       ExportJob
  common/       Frame, Rect2f, Logging
tests/          tests unitarios (CTest)
research/       scripts Python de prototipo/benchmark (no runtime de la app)
docs/           research.md, ARCHITECTURE.md, licencias
third_party/    dependencias fuente (ej. vid.stab, próximamente libraw)
resources/      iconos, estilos
```

## Convenciones

- C++17. Sin comentarios innecesarios en código nuevo salvo que se pidan.
- Seguir patrones existentes del archivo vecino antes de crear patrones nuevos.
- NO añadir dependencias sin verificar licencia y compatibilidad GPLv3.
- Todo frame de salida debe conservarse (regla de negocio central del proyecto).
- Interfaz: no bloquear el hilo de UI; procesamiento en worker thread.

## No hacer

- No ejecutar ffmpeg.exe como dependencia de producción.
- No plate solving, ni astronomía, ni IA en el MVP.
- No stack, wavelets, deconvolution, ni calidad planetaria en el MVP.