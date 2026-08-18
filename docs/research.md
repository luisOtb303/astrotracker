# research.md — Investigación previa y decisiones de diseño

Estado: Fase 0/1. Documenta el porqué de las decisiones técnicas de AstroTracker.

## Objetivo del proyecto

Seguir y estabilizar el **Sol o la Luna** en vídeos grabados con DSLR/mirrorless
sobre trípode manual. El movimiento de la cámara (manual o deriva de campo) hace
que el objeto se mueva de frame a frame. AstroTracker aplica un "star tracker
virtual": sigue el objeto y desplaza cada frame para mantenerlo centrado.

Restricciones clave del producto:

- **Nunca descartar frames** (regla de negocio central). Si el tracking se pierde,
  se predice con el modelo de movimiento y el frame se conserva igualmente.
- **Sin plate solving, sin astronomía, sin IA** en el MVP. No se necesita saber qué
  objeto es.
- Se elige una ROI manualmente y se sigue.

## Herramientas y proyectos estudiados

### PIPP (freeware, cerrado)

- Herramienta de preprocesado para astrofotografía planetaria/lunar.
- Modelo conceptual relevante: preprocesado de secuencias, centrado por tracking.
- Problema: es **freeware cerrado**, no reutilizable, y su modelo de centrado
  planetario no encaja con el caso "Sol/Luna con fondo negro".
- Conclusión: solo referencia conceptual. No copiar código ni pipeline.

### vid.stab

- Biblioteca de estabilización de vídeo (GPL-2.0-or-later) usada por muchos NLE.
- Estima el **movimiento global** de la cámara (homografía global) usando
  features/flow.
- Problema para este caso: con cielo oscuro y un solo objeto brillante no hay
  features suficientes; la estimación global falla y puede producir artefactos.
- Conclusión: no es el núcleo. Se conserva como módulo **opcional** de
  "estabilización global" (terceros, `third_party/vid.stab`), fuera del MVP. El
  núcleo es object-tracking propio sobre el objeto.

### OpenCV

- Proporciona el entorno de visión: `cv::Mat`, `matchTemplate`, umbralización,
  centroide, Kalman.
- Los trackers CSRT/KCF/MOSSE viven en **opencv_contrib**, que **no** está en los
  prebuilt oficiales de opencv.org.
- Decisión: usar TemplateTracker + CentroidTracker (ambos en el prebuilt) como
  núcleo. CSRT queda como opción futura si se compila opencv_contrib; **no** se
  asume como solución definitiva.

### FFmpeg/libav

- Decodificación/codificación de vídeo integrada como **librería**
  (libavformat/libavcodec/libswscale), nunca ejecutando `ffmpeg.exe` en producción.
- El dev-package de gyan.dev (build GPL con libx264) proporciona `.lib` de
  importación MSVC compatibles con Visual Studio.
- Interfaz propia `IVideoReader`/`IVideoWriter` para poder sustituir la
  implementación (p. ej. SER) sin tocar el resto.

### SER Player (MIT)

- Reutilizable bajo MIT: protocolo SER v3 (formato de frames sin comprimir usado
  por cámaras ZWO/QHY).
- Referencia para el soporte de lectura/escritura SER en fases posteriores.

### Siril (GPL-3.0)

- Solo referencia conceptual de flujos astronómicos. No se copia código.

## Decisiones de arquitectura

1. **Dos pasadas**: (1) analizar/seguir/calcular transforms → archivo de
   transformación; (2) aplicar y codificar. Permite re-exportar sin re-trackear.
2. **Pipeline** en `processing/`: se conservan todos los frames; el stabilizer
   desplaza por traslación (XY) para centrar el objeto en un punto objetivo.
3. **Modelo de movimiento**: Kalman (x, y, vx, vy) con estados
   `VALID / UNCERTAIN / LOST` para predecir durante oclusiones.
4. **No bloquear el hilo de UI**: procesamiento en worker thread.
5. **Terceros**: solo librerías con licencia compatible GPLv3 (FFmpeg GPL, Qt
   LGPLv3/GPLv3, OpenCV Apache-2.0, vid.stab GPL-2.0-or-later, SER Player MIT).

## Fases

- Fase 0: investigación, decisiones, toolchain. ✔
- Fase 1: visor de vídeo (abrir/reproducir/frame a frame/ROI). ✔
- Fase 2: tracking (ITracker, TemplateTracker, CentroidTracker, Kalman,
  MotionModel, TrackStatus).
- Fase 3: estabilización por traslación (Stabilizer, SmoothingFilter).
- Fase 4: pipeline de dos pasadas y exportación (FFmpegVideoWriter, SER).
- Fase 5: UX de preview y parámetros.

## Estado actual

- Fase 1 completada (commit `7171c77`): `FFmpegVideoReader` funcional, visor con
  reproducción y selección de ROI, smoke test `tests/smoke_reader`.
