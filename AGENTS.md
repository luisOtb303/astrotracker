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
- Trackers: TemplateTracker y CentroidTracker son los principales (vídeo);
  el modo Fotos usa el núcleo propio DiscTracker/CircleEstimator (radio fijo).
  CSRT es opcional (requiere opencv_contrib) y NO se asume como solución definitiva
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
  stills/       PhotoSequenceReader (secuencias de fotos; RAW con LibRaw)
  raw/          RawDecoder (LibRaw: CR2/CR3, DNG, NEF, ARW…) *
  tracking/     ITracker, TemplateTracker, CentroidTracker, OpticalFlowTracker, HybridTracker
  motion/       MotionModel, Kalman, TrackStatus (VALID/UNCERTAIN/LOST)
  stabilization/ Stabilizer, TargetPosition, SmoothingFilter
  processing/   Pipeline, FrameTransformer, BorderHandler, Preprocessor
  export/       ExportJob
  common/       Frame, Rect2f, Logging
tests/          tests unitarios (CTest)
research/       scripts Python de prototipo/benchmark (no runtime de la app)
docs/           research.md, ARCHITECTURE.md, licencias
third_party/    dependencias fuente (vid.stab; libraw, vendido en-tree)
resources/      iconos, estilos
```

## Convenciones

- C++17. Sin comentarios innecesarios en código nuevo salvo que se pidan.
- Seguir patrones existentes del archivo vecino antes de crear patrones nuevos.
- NO añadir dependencias sin verificar licencia y compatibilidad GPLv3.
- Todo frame de salida debe conservarse (regla de negocio central del proyecto).
- Interfaz: no bloquear el hilo de UI; procesamiento en worker thread.
- **Versionado y "Acerca de"**: cada cambio de **minor/major** (semver) debe
  incrementar `project(... VERSION x.y.z)` en `CMakeLists.txt` y añadir la
  entrada correspondiente al changelog. La versión de la app y el diálogo
  **Ayuda > Acerca de** se propagan automáticamente desde CMake
  (`ASTROTRACKER_VERSION`, `ASTROTRACKER_GIT_REV`, `ASTROTRACKER_BUILD_TYPE`);
  no hardcodear versiones en el código. Los parches internos no cambian la
  versión salvo que haya release. Si cambia el autor/copyright, actualizar
  `src/ui/AboutDialog.cpp`. Al añadir/quitar una dependencia, mantener al día
  `THIRD_PARTY_LICENSES/index.md` y la pestaña Licencias del `AboutDialog`
  (textos en `resources/licenses/`).
- **Criterio de versiones**: **minor** por cada lote de features publicado
  (0.1.0 → 0.2.0 → 0.3.0…); **patch** solo para releases de correcciones
  (0.2.1); **major** para hitos grandes (p. ej. detector automático renovado o
  integración de IA → candidato natural a 1.0.0).
- **Proceso de release** (checklist): 1) bump `project(VERSION)`; 2) mover
  `[Unreleased]` a una sección nueva del changelog con fecha; 3) commit;
  4) tag anotado `vx.y.z` sobre ese commit (así el ejecutable lleva el rev
  correcto); 5) regenerar artefactos con `scripts/make-release.ps1`
  (build + ctest + `cpack -C Release`: ZIP portable + instalador NSIS).
  El instalador requiere NSIS (`winget install NSIS.NSIS`) solo para generarlo.

## No hacer

- No ejecutar ffmpeg.exe como dependencia de producción.
- No plate solving, ni astronomía, ni IA en el MVP.
- No stack, wavelets, deconvolution, ni calidad planetaria en el MVP.