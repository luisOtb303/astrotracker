# Roadmap futuro

Líneas de trabajo valoradas y aparcadas, para retomarlas tras el release
0.2.0 / publicación en GitHub. Ninguna está comprometida: cada una se
re-evaluará (con perfilado/métricas) antes de empezar.

---

## Línea A — Aceleración GPU

**Motivación**: la exportación MP4 es hoy casi toda CPU (`FFmpegVideoWriter`
usa `libx264`) y la decodificación de vídeo grande (1080p+/4K) también corre
en CPU. El procesamiento por foto (`DiscTracker`, `CircleEstimator`,
LibRaw) NO es candidato: operaciones pequeñas o ya limitadas por E/S.

### G1 — HW accel en vídeo (prioridad media, mejor relación impacto/riesgo)

- **Decodificar**: activar hwaccel en `FFmpegVideoReader` con `d3d11va`
  (nativo de Windows; alternativas `dxva2`, `nvdec`). Fallback automático a
  CPU si no hay GPU/dispositivo.
- **Codificar**: encoder hardware seleccionable en la exportación MP4:
  `h264_nvenc` (NVIDIA), `h264_qsv` (Intel), `h264_amf` (AMD), con
  autodetección y fallback a `libx264`. Requiere que el build de FFmpeg los
  incluya (los *full* de gyan.dev los traen).
- **Coste de distribución**: cero dependencias nuevas en el instalador — los
  decoders/encoders usan los drivers del usuario.

### G2 (opcional) — Visión con OpenCL

- `cv::UMat` (T-API OpenCL) funciona con la OpenCV prebuilt actual y cualquier
  GPU moderna (NVIDIA/AMD/Intel) con fallback automático a CPU.
- Candidato claro: `matchTemplate` del TemplateTracker y de la re-adquisición
  M3 sobre fotos grandes (24-45 MP).

### Descartado por ahora

- Compilar OpenCV con módulo `cuda`: requiere CUDA Toolkit + redistribuir el
  runtime (cientos de MB) para ganancia marginal frente a UMat en nuestro caso.

### Riesgos a tener presentes

- Transferencia CPU↔GPU por frame: solo compensa en operaciones grandes.
- Matriz de hardware: detección + fallback obligatorio (la app nunca debe
  fallar por falta de GPU).
- Redondeos GPU≠CPU: los tests de regresión deberían usar tolerancias si un
  resultado puede venir de camino acelerado.

---

## Línea B — Detección automática con IA

**Motivación**: el modo automático falla más de lo deseable. Las heurísticas
actuales (gradiente del limbo, blob brillante, plantilla) rompen cuando su
suposición "borde limpio" no se cumple: crecientes finos, nubes, ocultación
parcial (árboles/montaña/horizonte), cambios de exposición entre fotos.
PIPP/AutoStakkert "resuelven" esto **descartando** los frames malos, que es
justo lo contrario de la regla central del proyecto (nunca descartar frames).
La analogía válida es la imagen médica: una CNN localiza lesiones donde las
heurísticas clásicas son frágiles; hay precedente directo en astronomía solar
(detección de manchas/eventos con U-Net, Mask R-CNN, SSD).

### Enfoque: la IA propone, lo clásico dispone

- La IA sería **un candidato más** dentro del sistema actual de candidatos
  (predicción, plantilla, arco del blob, blob simétrico), elegido como hoy por
  calidad de limbo; el ajuste fino seguiría siendo clásico (banda radial +
  `CircleEstimator`): subpíxel, determinista y testeable.
- La regla "nunca descartar frames" queda intacta: la IA solo logra que haya
  más frames válidos.
- Inferencia vía **`cv::dnn`** (ya incluido en OpenCV, Apache-2.0): carga ONNX,
  corre en CPU de cualquier máquina en milisegundos (modelo pequeño ~2-5 MB,
  entrada ~512 px); en máquinas con GPU mejor se acelera automáticamente.
  Fallback garantizado. Sin dependencias nuevas.

### Datos (el reto real; tenemos ventaja)

- **Sintéticos**: generador de soles/lunas con fase, nubes, ruido, JPEG,
  desenfoque, fondos (árboles/montañas) → miles de ejemplos etiquetados gratis.
- **Reales sin esfuerzo**: cada corrección manual del usuario ya es una
  etiqueta (el círculo corregido queda guardado en el proyecto `.atracker`);
  añadir exportador de dataset desde proyectos + `testdata/eclipse`.
- Volumen necesario modesto: tarea muy restringida (localizar un disco).

### Entrenamiento

- Offline en `research/` (Python/PyTorch; nunca se distribuye Python ni el
  pipeline de entrenamiento). CPU local o Colab gratuito bastan para un modelo
  pequeño (heatmap de centro + radio); en máquinas con NVIDIA se acelera.

### Fases propuestas (medir antes de tocar)

1. **B0 — Corpus + métricas**: extender `tests/test_eclipse` a medidor
   objetivo (% válidas/supuestas/perdidas por carpeta + overlays). Baseline
   del estado actual.
2. **B1 — Pulido clásico** iterando contra el corpus: CLAHE antes del barrido
   radial, búsqueda multi-escala de plantilla, pesos del arco visible,
   umbrales adaptativos. Suele recuperar buena parte sin IA.
3. **B2 — Datos**: generador sintético + exportador de etiquetas reales.
4. **B3 — Entrenar + benchmark**: IA vs clásico sobre el corpus.
5. **B4 — Integración condicionada**: candidato IA vía `cv::dnn` solo si gana
   al clásico en el benchmark; desactivable.

### Precondiciones

- Revisar la decisión técnica fijada en `AGENTS.md` ("no IA en el MVP") al
  adoptar esta línea, con la justificación correspondiente.
- Prioridad relativa: después del release 0.2.0 y la publicación en GitHub;
  B0/B1 antes que cualquier cosa de IA.
