# Changelog

Todas las modificaciones notables de AstroTracker se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es/1.1.0/) y el
versionado es [SemVer](https://semver.org/lang/es/) (MAJOR.MINOR.PATCH).

## [Unreleased]

### Added

- **Perfiles de seguimiento y métodos de auto-centrado (fase 1)**: nuevo dock
  "Seguimiento" con **perfil de objeto** (Auto/Sol/Luna/Planeta/Eclipse solar/
  Eclipse lunar) que define la cadena de prioridad de métodos, **override del
  método principal por foto** ("Método de esta foto"), estado en vivo de la
  foto actual (método · confianza · validez) y los ajustes de vídeo (antes en
  la toolbar). El motor se reorganizó en detectores intercambiables
  (`IDiscDetector`) con fusión por soporte radial del limbo; cada resultado
  guarda **confianza y método**, visibles en la Salida, el filmstrip y el
  proyecto. Métodos nuevos:
  - **Radio conocido**: círculo de radio fijo sobre el contorno umbralizado,
    aceptando arcos cortos y anillos de totalidad vía área ≈ πR² (la técnica
    de los alineadores de eclipse); prioritario en los perfiles de eclipse.
  - **Correlación de fase**: desplazamiento global por FFT entre el parche de
    referencia confirmado y la ventana actual, subpíxel; fallback cuando el
    limbo no es confirmando.
  - **Centroide**: centroide del blob si es compatible con disco lleno
    (planetas); rechaza crecientes.
- **Caché de análisis para RAW**: la primera vez que se lee un CR2/CR3 (con
  demosaicing completo) se guarda una copia pequeña (JPG 1600 px) en
  `_astrotracker_cache/` junto a las fotos; la navegación, las miniaturas
  grandes y el cálculo automático salen desde ahí (la carpeta se puede borrar
  sin riesgo: se regenera sola, se invalida si cambia la foto original y las
  entradas llevan versión para regenerarse si cambia el procesado; medido en
  el banco del eclipse: segunda pasada ~6× más rápida). La exportación sigue
  usando la resolución completa original.
- **Motor Disco unificado en modo Vídeo**: el pipeline de vídeo ahora puede
  usar el motor `DiscTracker` (el mismo del modo Fotos) sin necesidad de
  dibujar ROI; el modo automático estima radio y centro desde el mayor blob
  del primer frame (estimación de área + barrido de arco), y los detectores
  del perfil seleccionado (Template, ArcBlob, PhaseCorrelation, Ecc, Features,
  KnownRadius, Centroid) se aplican frame a frame con fusión por soporte del
  limbo.
- **Botones de vídeo en el dock**: Seguir, Vista previa y Exportar ahora viven
  en el dock "Seguimiento" > Vídeo como botones con icono, sin toolbar
  dedicada.
- **Selector "Disco (perfil)" en el combo de tracker de vídeo**: permite usar
  el motor de Fotos con el perfil activo (Sol, Luna, Planeta, Eclipse, etc.)
  directamente desde el modo vídeo.

### Fixed

- **El visor ya no "aclara" ni pixela las fotos oscuras**: había DOS ajustes
  de brillo por foto distorsionando la secuencia. El nuestro (conversión 16→8
  con estirado min-max por foto) y el de dentro de LibRaw (el auto-brillo de
  dcraw, que fija la ganancia según el percentil del histograma de CADA foto:
  las oscuras salían amplificadas, con el ruido de croma convertido en
  moteado naranja). Ahora todo el pipeline es fiel y determinista: LibRaw sin
  auto-brillo (`no_auto_bright`) y con demosaicing completo, y mapeo 16→8
  fijo (dividir por 257), idéntico en visor, análisis y exportación.
- **Normalización de brillo del MP4 con tope**: la ganancia estaba sin límite
  y las fotos mucho más oscuras que la primera (totalidad del eclipse) se
  saturaban hasta verse pixeladas. Ahora el ajuste se limita a ×3 como máximo
  y la Salida avisa cuando se recorta.
- **Siembra automática robusta**: la detección inicial en el primer frame del
  modo Disco ahora estima el radio del disco desde el área del blob (mucho más
  preciso que un porcentaje fijo del ancho de imagen), evitando desalineaciones
  significativas en discos pequeños o alejados.

## [0.2.0] - 2026-08-21

### Added

- **Proyectos guardables (`.atracker`)** — guardar y retomar el trabajo:
  - **Archivo > Abrir proyecto...** (`Ctrl+Mayús+O`), **Cerrar proyecto**
    (`Ctrl+W`), **Guardar proyecto** (`Ctrl+S`) y **Guardar proyecto como...**
    (`Ctrl+Mayús+S`), también con botones en la barra de herramientas; menú
    Archivo reordenado al uso clásico (abrir → cerrar → guardar → recientes,
    "Salir" al final); título con el nombre del proyecto y asterisco si hay
    cambios sin guardar, y aviso *"¿Guardar los cambios?"* al cerrar, empezar
    de nuevo o abrir otro trabajo.
  - El proyecto guarda el trabajo del modo **Fotos** (resultados del
    seguimiento foto a foto, fotos bloqueadas y corregidas a mano, selección
    de exportación, semilla y foto actual) y del modo **Vídeo** (vídeo abierto,
    ROI, ajustes del pipeline y posición). Formato JSON legible
    (`stills/PhotoProject`), testeado en `tests/test_project`.
  - Al reabrir, los resultados se emparejan **por nombre de archivo**: se
    tolera mover la carpeta de fotos o añadir/quitar fotos sin perder el
    trabajo; las fotos ausentes se avisan por la Salida.
  - **Autoguardado**: tras cada cambio se reescribe `proyecto.atracker.bak`
    silenciosamente; si al abrir el proyecto ese archivo es más reciente que
    el principal, la Salida avisa para recuperarlo renombrándolo.
  - Sección **"Proyectos"** en el menú Recientes.

## [0.1.0] - 2026-08-20

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
- Fase 3 — Estabilización por traslación:
  - `TargetPosition`: punto objetivo (por defecto, centro del frame) donde se
    mantiene el objeto.
  - `SmoothingFilter`: EMA sobre el centro del objeto para atenuar el jitter.
  - `Stabilizer`: calcula el desplazamiento (dx, dy) que centra el objeto
    (`warpAffine` con bordes negros) y expone `target`/`offset`.
  - `tests/test_stabilizer` (CTest): convergencia del offset, centrado del
    objeto tras `apply` y atenuación del jitter.
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
- Fase 6 — Modo "Fotos" (centrado de secuencias de fotos; M1):
  - `stills/PhotoSequenceReader`: lee una secuencia de fotos
    (JPG/PNG/TIFF/BMP) por carpeta o lista, con **orden natural de nombres**
    (IMG_2 antes que IMG_10), decodificación bajo demanda, `readAt` con
    redimensión para vista previa/análisis (normaliza 16-bit → BGR8) y
    `readFullRes` para exportación sin tocar la imagen.
  - `ui/PhotoPanel`: pestaña "Fotos" con **filmstrip de miniaturas**, dos
    visores (Original | Centrado), slider y navegación foto a foto; acciones
    "Abrir carpeta" y "Abrir fotos...".
  - `MainWindow`: central con pestañas **Vídeo | Fotos** (el modo vídeo queda
    intacto) y acción de menú "Abrir fotos (secuencia)...".
  - `tests/test_stills` (CTest): orden natural, descarte de archivos no
    soportados, geometría, normalización 16-bit, maxDim y `readFullRes`.
  - `testdata/photos/`: secuencia de ejemplo (10 fotos con el disco móvil,
    extraídas de `moon.mp4`).
- Fase 6 — Soporte RAW en el modo "Fotos" (M2):
  - **LibRaw 0.22.2 vendido en `third_party/libraw`** (LGPL-2.1/CDDL) con wrapper
    CMake propio que compila la librería estática en Windows/MSVC (defines
    `LIBRAW_BUILDLIB` públicos para evitar `__declspec(dllimport)`).
  - `raw/RawDecoder`: reconocimiento de cabecera (`isRawFile`), dimensiones,
    **decodificación 8-bit (BGR8, mapeo de tono) y 16-bit (CV_16UC3 sin
    comprimir**, para exportación) con `half_size` y `maxDim`, y miniatura
    embebida (JPEG/BITMAP) para filmstrips.
  - `stills/PhotoSequenceReader` acepta **CR2/CR3/DNG/NEF/ARW/ORF/RAF/RW2/PEF/SRW/
    RAW**: las miniaturas del filmstrip usan la miniatura embebida del RAW y
    `readFullRes` decodifica a 16-bit; `probeSize` lee dimensiones de cabecera.
  - `ui/PhotoPanel`: el diálogo de apertura ahora acepta los formatos RAW.
  - `tests/test_raw` (CTest): smoke test con **RAW reales CC0
    (`testdata/raw/`): Canon 40D sRAW2 (.CR2) y Canon R6 (.CR3)** — verifica
    cabecera, dimensiones, thumbnail y decode 8/16-bit.
- Fase 6 — Seguimiento del disco en el modo "Fotos" (M3):
  - `common/CircleF`: círculo (centro + radio) en píxeles de la imagen de trabajo.
  - `tracking/CircleEstimator`: ajuste robusto de círculo de **radio fijo** con
    prior del centro (proyección iterativa + descarte de outliers); maneja
    arcos parciales (creciente, eclipse parcial, sol tras montaña).
  - `tracking/DiscTracker`: sigue el centro del disco foto a foto buscando el
    limbo (paso brillante→oscuro) en una banda radial alrededor del centro
    predicho por el modelo de movimiento; estados VALID/UNCERTAIN/LOST y
    posición "supuesta" cuando el objeto está oculto (nube, montaña).
  - `ui/VideoView`: modo círculo (pintar desde el centro, mover arrastrando el
    centro, redimensionar arrastrando el borde) con pintado sólido o
    discontinuo (predicho); sin tocar el ROI del modo vídeo.
  - `ui/PhotoPanel`: **modos de dibujo conmutables** (Círculo | Rectángulo, el
    círculo se ajusta inscrito), botón "Seguir secuencia" operativo, worker en
    hilo propio (`stills/PhotoTrackWorker`) que sigue la secuencia en **ambas
    direcciones desde la foto sembrada** sin bloquear la UI, **visor
    "Centrado"** con el centro del círculo en el centro del visor (relleno de
    bordes Borde negro/Réplica) y corrección manual: arrastrar el círculo en
    cualquier foto re-siembra y vuelve a seguir desde ahí.
  - Fase 6 (M3) — **Re-adquisición del disco por plantilla**: cuando el Sol/Luna
    salta entre fotos más de lo que abarca la banda radial (deriva típica sin
    star tracker), el seguimiento compara cada foto contra el parche del círculo
    de la última foto confirmada (`matchTemplate`) y amplía la ventana de
    búsqueda progresivamente hasta volver a localizar el disco, en lugar de
    quedarse con la predicción congelada en el círculo de la semilla.
  - Fase 6 (M3) — **Centrado del disco por el arco visible** (`tracking/DiscArcFit`):
    el centro del disco se obtiene del **círculo que forma la fase visible** — en
    crecientes, menguantes, eclipse parcial y corona — no del punto más brillante
    ni del blob. El seguimiento genera candidatos (predicción, plantilla, arco del
    blob, blob simétrico) y elige el que muestra el **limbo radial más nítido**;
    así el círculo rojo queda centrado en el disco real aunque el Sol/Luna esté
    parcialmente oculto o la fase cambie de forma entre fotos.
  - Fase 6 (M3) — **"Ajustar fotograma"** en el modo Fotos: detecta el disco
    **solo en la foto actual** (barrido de radio + arco visible) y ajusta el
    círculo, sin tocar el resto de la secuencia.
  - Fase 6 (M3) — **Botones del modo "Fotos" simplificados**: **"Calcular
    automáticamente"** recorre **todas las fotos** desde el círculo sembrado;
    **"Bloquear fotograma"** fija la foto actual para que el cálculo automático no
    la modifique (etiqueta azul "bloqueada" en el filmstrip). La edición del
    círculo sobre una foto ya analizada es **siempre manual** (solo actualiza esa
    foto); se elimina el modo "Auto" conmutable y el re-seguido automático.
- Fase 6 — **Exportación de las fotos centradas** (`stills/PhotoExportWorker`):
  el botón **"Exportar centradas..."** ofrece **Fotos JPG / Fotos PNG** (carpeta
  `centrada_0000.jpg`…) o **Vídeo MP4** (H.264, FPS configurable libre con
  presets 5/10/24/30, por defecto 10), en resolución **Original**, la del visor
  (1600 px) o un estándar de vídeo (**HD, FHD, 2K, 4K**, con relleno negro para
  mantener la relación de aspecto). Las fotos sin resultado válido se exportan
  igualmente sin desplazar (regla: nunca descartar frames). RAW a 16 bits se
  convierte a 8 bits en la salida.
- Fase 6 — **Selección de fotos a exportar**: cada miniatura del filmstrip tiene
  una **casilla** (marcada por defecto); clic en la casilla solo la marca/desmarca
  (sin navegar) y la exportación respeta esa selección. Se mantiene el estado
  "bloqueada" (que protege del cálculo automático) independiente de la casilla.
- Fase 6 — **Panel de salida detallado** (`common/AppLog` + dock "Salida" en
  MainWindow): registro en color (info/aviso/error/debug) con líneas por foto
  ("procesando IMG_4093.cr2 → centrando", "foto 3/300 · IMG_4095.cr2 → válida
  (…)", re-adquisiciones, resúmenes), conmutable desde el menú **Ver**, botón
  **Vaciar** y casilla **Depuración**. La barra de estado muestra durante el
  proceso el **nombre del archivo** además del nº de foto.
- Fase 6 — **Botón "Nuevo"** en el modo Fotos: cierra la secuencia actual y
  limpia todos los resultados (con confirmación si hay trabajo).
- Fase 6 — **Carga asíncrona de fotos** (`stills/PhotoFrameLoader`): al navegar
  entre fotos (RAW incluidas) el visor muestra la miniatura con el indicador
  **"Abriendo foto…"** mientras se decodifica en segundo plano; la UI no se
  bloquea y las peticiones encadenadas se coalescen a la última foto pedida.
- Fase 6 — **Círculo del disco en las miniaturas**: cada miniatura del filmstrip
  dibuja encima el círculo del seguimiento (verde continuo = válida, discontinuo
  = supuesta), escalado al tamaño de la miniatura, para localizar de un vistazo
  las fotos mal seguidas.
- Fase 6 — **Las correcciones manuales se respetan**: editar/ajustar el círculo
  de una foto la marca como **fijada**; volver a pulsar "Calcular
  automáticamente" ya no sobrescribe esas fotos (igual que las bloqueadas), solo
  revisa el resto.
- Fase 6 — **Suavizado de transiciones en la exportación MP4**: opción en el
  diálogo "Exportar centradas..." (solo vídeo) con **fotogramas intermedios**
  por par de fotos (0–4, por defecto 2) y **normalización de brillo** (por
  defecto activada) que evita el parpadeo entre tomas. Los intermedios son un
  **fundido cruzado de los frames ya centrados** (el disco queda en el centro
  en todo momento; solo el fondo se desliza). Total ≈ N + (N−1)·intermedios
  fotogramas al FPS elegido.
- Fase 6 (M3) — El filmstrip etiqueta cada miniatura como **válida /
  supuesta / dudosa** con el color del estado.
- Fase 6 — **Diagnóstico sobre las fotos del eclipse reales**: `tests/test_eclipse`
  es un harness opcional (no falla, solo informa) que corre el motor contra los
  CR2 de `testdata/eclipse/` y escribe overlays (verde = semilla/Otsu, rojo =
  seguido) en `<carpeta>/_props`, para revisión visual del seguimiento.
- **Menú "Ayuda" y diálogo "Acerca de"** (`ui/AboutDialog`): "Ayuda > Acerca de
  AstroTracker" muestra **versión** (semver, desde CMake), **revisión git**
  (commit corto), **compilación** (configuración + fecha/hora de build) y
  copyright; "Ayuda > Licencias" lista los componentes de terceros con los
  **textos completos de las licencias** (GPLv3, LGPLv3, Apache-2.0, GPLv2,
  LGPL-2.1, CDDL-1.0) embebidos como recursos Qt en `resources/licenses/`;
  "Acerca de Qt" abre el diálogo estándar.
- **Versión centralizada en CMake**: `project(... VERSION)` propaga
  `ASTROTRACKER_VERSION`, `ASTROTRACKER_BUILD_TYPE` y `ASTROTRACKER_GIT_REV` al
  ejecutable y al "Acerca de" (se elimina la versión hardcodeada en `main.cpp`).
- **Icono de la aplicación**: `resources/favicon/app.ico` (multi-tamaño
  16–256 px, regenerado desde el PNG 512) incrustado en el `.exe` (Explorador)
  vía `resources/app.rc`, y `app-icon-512.png` como icono de ventana/taskbar
  (`setWindowIcon`) y en el "Acerca de".
- **Empaquetado (CPack)**: `cpack -C Release` genera en `build/` dos artefactos:
  - `AstroTracker-<versión>-win64.zip`: versión **portable** (descomprimir y
    ejecutar) con ejecutable, DLLs de Qt/FFmpeg/OpenCV con sus plugins, README,
    `THIRD_PARTY_LICENSES` y textos de licencia; solo necesita CMake
    (libarchive integrado).
  - `AstroTracker-<versión>-win64.exe`: **instalador NSIS** que instala en
    Program Files, crea el acceso del menú Inicio y registra la desinstalación
    en Configuración > Aplicaciones (muestra la GPLv3 en el asistente y ofrece
    desinstalar la versión previa al actualizar). Requiere NSIS para generarlo.
  En ambos se excluye la DLL de depuración de OpenCV (`opencv_world*d.dll`),
  listo para publicar como GitHub Releases.

### Changed

- **La app ya no abre la ventana negra de consola**: el ejecutable se vincula
  como GUI Windows (`add_executable(... WIN32)`), que es lo correcto para una
  aplicación Qt; antes quedaba como consola y Windows abría un `cmd` además de
  la ventana.
- **La app recuerda la última carpeta y mantiene "Recientes"**: los diálogos de
  apertura (vídeo y fotos) vuelven a abrir en la última carpeta usada, y el menú
  Archivo → *Recientes* lista los últimos vídeos abiertos y las últimas carpetas
  de fotos (máx. 8 de cada), para reabrirlos con un clic. Persistencia vía
  `QSettings` (se fija `organizationName` en `main.cpp`).
- Barra de estado de operaciones en el modo "Fotos": el seguimiento automático
  y las operaciones lentas ya **no abren ventanas emergentes ni cambian el
  cursor a reloj**. El progreso foto a foto («Procesando foto X/Y…»), la
  generación de miniaturas del filmstrip y el resumen final se muestran en la
  barra de estado inferior (con la barra de progreso pequeña de la ventana).
  El "Cancelar" pasa a un botón **"Detener"** en la barra de Fotos, activo solo
  durante el seguimiento.

### Fixed

- `FFmpegVideoReader::convertFrame`: el constructor de `cv::Mat` recibía
  `(width, height)` en orden inverso (rows/cols), produciendo frames
  transpuestos (480x640 en lugar de 640x480).
- `Kalman::correct`: la ganancia se calculaba con matrices 4×4 singulares
  (la medición es 2D); ahora se usa matemática 2×2 y el filtro converge
  correctamente a posición y velocidad.
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
- `Pipeline::run`: se alinean los offsets con `startUs` (los frames anteriores
  al inicio del seguimiento se conservan sin transformar, sin descartar nada).
- `tests/test_pipeline`: se añade `CentroidTracker.cpp` al target (el pipeline
  ahora lo instancia).
- Fase 6 — **Edición del círculo tras la carga asíncrona**: al re-navegar a una
  foto ya cargada, el loader no volvía a entregarla (solo emitía cuando el
  índice pedido cambiaba) y el visor se quedaba atascado en la miniatura con
  "Abriendo foto…"; además, editar el círculo sobre esa miniatura guardaba las
  coordenadas en espacio de miniatura (240 px) como si fueran del análisis
  (1600 px), encogiendo el círculo hacia la esquina superior izquierda. Ahora
  el loader despierta por contador de peticiones y cachea el último frame
  (pedir la misma foto la re-entrega al instante) y la edición del círculo/ROI
  queda **deshabilitada mientras se carga** (solo se edita sobre el frame a
  resolución completa).
- Fase 6 — **Fotogramas intermedios de la exportación MP4**: el morph desplazaba
  los dos frames centrados en sentidos opuestos para alinear el fondo, con lo
  que en cada transición el disco aparecía **doble/desplazado** (un sol fantasma
  separado por el desplazamiento entre fotos) y el vídeo parecía exportado desde
  el original sin centrar. Ahora los intermedios son un **fundido cruzado de los
  frames ya centrados** y el disco queda en el centro exacto en todos los
  fotogramas. Regresión cubierta por `tests/test_export` (JPG y MP4 con
  intermedios sobre fotos sintéticas con centros conocidos).