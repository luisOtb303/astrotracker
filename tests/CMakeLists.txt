# Smoke test: FFmpegVideoReader decodifica un vídeo real y hace seek.

add_executable(smoke_reader smoke_reader/main.cpp
  ../src/common/Frame.h
  ../src/video/IVideoReader.h
  ../src/video/FFmpegVideoReader.h
  ../src/video/FFmpegVideoReader.cpp)

target_include_directories(smoke_reader PRIVATE ../src)
target_compile_definitions(smoke_reader PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(smoke_reader PRIVATE
  ${OpenCV_LIBS}
  FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale)

find_file(MOON_VIDEO moon.mp4
  PATHS ${CMAKE_SOURCE_DIR}/testdata
  NO_DEFAULT_PATH)

if(MOON_VIDEO)
  add_test(NAME smoke_reader COMMAND smoke_reader ${MOON_VIDEO})
endif()

# Test del modelo de movimiento (Kalman + MotionModel).

add_executable(test_motion test_motion/main.cpp
  ../src/motion/TrackStatus.h
  ../src/motion/Kalman.h
  ../src/motion/Kalman.cpp
  ../src/motion/MotionModel.h
  ../src/motion/MotionModel.cpp)

target_include_directories(test_motion PRIVATE ../src)
target_compile_definitions(test_motion PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_motion PRIVATE ${OpenCV_LIBS})

add_test(NAME test_motion COMMAND test_motion)

# Test de trackers sobre frames sintéticos (disco brillante móvil).

add_executable(test_trackers test_trackers/main.cpp
  ../src/tracking/ITracker.h
  ../src/tracking/TemplateTracker.h
  ../src/tracking/TemplateTracker.cpp
  ../src/tracking/CentroidTracker.h
  ../src/tracking/CentroidTracker.cpp)

target_include_directories(test_trackers PRIVATE ../src)
target_compile_definitions(test_trackers PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_trackers PRIVATE ${OpenCV_LIBS})

add_test(NAME test_trackers COMMAND test_trackers)

# Test del estabilizador por traslación (Stabilizer + SmoothingFilter).

add_executable(test_stabilizer test_stabilizer/main.cpp
  ../src/stabilization/TargetPosition.h
  ../src/stabilization/SmoothingFilter.h
  ../src/stabilization/SmoothingFilter.cpp
  ../src/stabilization/Stabilizer.h
  ../src/stabilization/Stabilizer.cpp
  ../src/processing/BorderHandler.h
  ../src/processing/BorderHandler.cpp)

target_include_directories(test_stabilizer PRIVATE ../src)
target_compile_definitions(test_stabilizer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_stabilizer PRIVATE ${OpenCV_LIBS})

add_test(NAME test_stabilizer COMMAND test_stabilizer)

# Test del writer FFmpeg: ida y vuelta de frames sintéticos.

add_executable(test_writer test_writer/main.cpp
  ../src/common/Frame.h
  ../src/video/IVideoReader.h
  ../src/video/FFmpegVideoReader.h
  ../src/video/FFmpegVideoReader.cpp
  ../src/video/IVideoWriter.h
  ../src/video/FFmpegVideoWriter.h
  ../src/video/FFmpegVideoWriter.cpp)

target_include_directories(test_writer PRIVATE ../src)
target_compile_definitions(test_writer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_writer PRIVATE
  ${OpenCV_LIBS}
  FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale)

if(WIN32)
  set(WRITER_TMP "$ENV{TEMP}/astrotracker_test_writer.mp4")
else()
  set(WRITER_TMP "${CMAKE_BINARY_DIR}/test_writer.mp4")
endif()
add_test(NAME test_writer COMMAND test_writer ${WRITER_TMP})

# Test del pipeline de dos pasadas (genera/estabiliza/verifica).

add_executable(test_pipeline test_pipeline/main.cpp
  ../src/common/Frame.h
  ../src/video/IVideoReader.h
  ../src/video/FFmpegVideoReader.h
  ../src/video/FFmpegVideoReader.cpp
  ../src/video/IVideoWriter.h
  ../src/video/FFmpegVideoWriter.h
  ../src/video/FFmpegVideoWriter.cpp
  ../src/tracking/DiscDetection.h
  ../src/tracking/DiscFusion.h
  ../src/tracking/DiscFusion.cpp
  ../src/tracking/DiscTracker.h
  ../src/tracking/DiscTracker.cpp
  ../src/tracking/DiscArcFit.h
  ../src/tracking/DiscArcFit.cpp
  ../src/tracking/DiscTrackerParams.h
  ../src/tracking/EccDiscDetector.h
  ../src/tracking/EccDiscDetector.cpp
  ../src/tracking/FeatureDiscDetector.h
  ../src/tracking/FeatureDiscDetector.cpp
  ../src/tracking/KnownRadiusDiscDetector.h
  ../src/tracking/KnownRadiusDiscDetector.cpp
  ../src/tracking/CentroidDiscDetector.h
  ../src/tracking/CentroidDiscDetector.cpp
  ../src/tracking/PhaseCorrelationDiscDetector.h
  ../src/tracking/PhaseCorrelationDiscDetector.cpp
  ../src/tracking/LimbScorer.h
  ../src/tracking/LimbScorer.cpp
  ../src/tracking/ArcBlobDiscDetector.h
  ../src/tracking/ArcBlobDiscDetector.cpp
  ../src/tracking/TemplateDiscDetector.h
  ../src/tracking/TemplateDiscDetector.cpp
  ../src/tracking/TrackingProfile.h
  ../src/tracking/CircleEstimator.h
  ../src/tracking/CircleEstimator.cpp
  ../src/tracking/IDiscDetector.h
  ../src/tracking/ITracker.h
  ../src/tracking/TemplateTracker.h
  ../src/tracking/TemplateTracker.cpp
  ../src/tracking/CentroidTracker.h
  ../src/tracking/CentroidTracker.cpp
  ../src/motion/TrackStatus.h
  ../src/motion/Kalman.h
  ../src/motion/Kalman.cpp
  ../src/motion/MotionModel.h
  ../src/motion/MotionModel.cpp
  ../src/stabilization/TargetPosition.h
  ../src/stabilization/SmoothingFilter.h
  ../src/stabilization/SmoothingFilter.cpp
  ../src/stabilization/Stabilizer.h
  ../src/stabilization/Stabilizer.cpp
  ../src/processing/BorderHandler.h
  ../src/processing/BorderHandler.cpp
  ../src/processing/FrameTransformer.h
  ../src/processing/Pipeline.h
  ../src/processing/Pipeline.cpp)

target_include_directories(test_pipeline PRIVATE ../src)
target_compile_definitions(test_pipeline PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_pipeline PRIVATE
  ${OpenCV_LIBS}
  FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale)

if(WIN32)
  set(PIPE_IN "$ENV{TEMP}/astrotracker_test_in.mp4")
  set(PIPE_OUT "$ENV{TEMP}/astrotracker_test_out.mp4")
else()
  set(PIPE_IN "${CMAKE_BINARY_DIR}/test_in.mp4")
  set(PIPE_OUT "${CMAKE_BINARY_DIR}/test_out.mp4")
endif()
add_test(NAME test_pipeline COMMAND test_pipeline ${PIPE_IN} ${PIPE_OUT})

# Test del lector de secuencias de fotos (orden natural, formatos, RAW vía LibRaw).

add_executable(test_stills test_stills/main.cpp
  ../src/stills/PhotoSequenceReader.h
  ../src/stills/PhotoSequenceReader.cpp
  ../src/stills/ExifReader.h
  ../src/stills/ExifReader.cpp
  ../src/raw/RawDecoder.h
  ../src/raw/RawDecoder.cpp
  ../src/raw/BayerHotPixels.h
  ../src/raw/BayerHotPixels.cpp
  ../src/raw/RawExifReader.h
  ../src/raw/RawExifReader.cpp)

target_include_directories(test_stills PRIVATE ../src)
target_compile_definitions(test_stills PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
  ASTROTRACKER_HAS_EXIV2)
target_link_libraries(test_stills PRIVATE ${OpenCV_LIBS} libraw exiv2lib)

add_test(NAME test_stills COMMAND test_stills)

# Smoke test de decodificación RAW real (CR2 y CR3 vía LibRaw).

add_executable(test_raw test_raw/main.cpp
  ../src/raw/RawDecoder.h
  ../src/raw/RawDecoder.cpp
  ../src/raw/BayerHotPixels.h
  ../src/raw/BayerHotPixels.cpp)

target_include_directories(test_raw PRIVATE ../src)
target_compile_definitions(test_raw PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_raw PRIVATE ${OpenCV_LIBS} libraw)

# Muestras de decode. Las de testdata/hotpixel llevan ademas pixeles quemados
# reales, asi que el test exige que el filtro corrija algo en ellas; las de
# testdata/raw solo tienen que salir intactas.
set(_raw_samples
  ${CMAKE_SOURCE_DIR}/testdata/raw/canon_40d_sraw2.CR2
  ${CMAKE_SOURCE_DIR}/testdata/raw/canon_r6.CR3
  ${CMAKE_SOURCE_DIR}/testdata/hotpixel/IMG_5633.CR2
  ${CMAKE_SOURCE_DIR}/testdata/hotpixel/IMG_6174.CR2)

# Cada muestra se carga solo si esta en disco. Los RAW de testdata/hotpixel son
# material de trabajo grande y pueden no estar presentes; test_raw deduce de la
# ruta si espera correcciones, asi que un subconjunto sigue siendo valido.
# Registrar el test "todo o nada" perderia la cobertura de las muestras que si
# existen.
set(RAW_SAMPLES)
foreach(_s IN LISTS _raw_samples)
  if(EXISTS ${_s})
    list(APPEND RAW_SAMPLES ${_s})
  endif()
endforeach()

if(RAW_SAMPLES)
  add_test(NAME test_raw COMMAND test_raw ${RAW_SAMPLES})
endif()

# Test del seguimiento del disco (DiscTracker + CircleEstimator) sobre
# discos sintéticos: lleno, creciente, sol tras montaña, oclusión total.

add_executable(test_disc test_disc/main.cpp
  ../src/motion/TrackStatus.h
  ../src/motion/Kalman.h
  ../src/motion/Kalman.cpp
  ../src/motion/MotionModel.h
  ../src/motion/MotionModel.cpp
  ../src/tracking/CircleEstimator.h
  ../src/tracking/CircleEstimator.cpp
  ../src/tracking/DiscArcFit.h
  ../src/tracking/DiscArcFit.cpp
  ../src/tracking/DiscDetection.h
  ../src/tracking/EccDiscDetector.h
  ../src/tracking/EccDiscDetector.cpp
  ../src/tracking/FeatureDiscDetector.h
  ../src/tracking/FeatureDiscDetector.cpp
  ../src/tracking/IDiscDetector.h
  ../src/tracking/LimbScorer.h
  ../src/tracking/LimbScorer.cpp
  ../src/tracking/DiscFusion.h
  ../src/tracking/DiscFusion.cpp
  ../src/tracking/TemplateDiscDetector.h
  ../src/tracking/TemplateDiscDetector.cpp
  ../src/tracking/CentroidDiscDetector.h
  ../src/tracking/CentroidDiscDetector.cpp
  ../src/tracking/KnownRadiusDiscDetector.h
  ../src/tracking/KnownRadiusDiscDetector.cpp
  ../src/tracking/PhaseCorrelationDiscDetector.h
  ../src/tracking/PhaseCorrelationDiscDetector.cpp
  ../src/tracking/ArcBlobDiscDetector.h
  ../src/tracking/ArcBlobDiscDetector.cpp
  ../src/tracking/DiscTracker.h
  ../src/tracking/DiscTracker.cpp)

target_include_directories(test_disc PRIVATE ../src)
target_compile_definitions(test_disc PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_disc PRIVATE ${OpenCV_LIBS})

add_test(NAME test_disc COMMAND test_disc)

add_executable(test_wb test_wb/main.cpp
  ../src/common/ImageAdjust.h
  ../src/common/ImageAdjust.cpp)

target_include_directories(test_wb PRIVATE ../src)
target_compile_definitions(test_wb PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_wb PRIVATE ${OpenCV_LIBS})

add_test(NAME test_wb COMMAND test_wb)

# Test de ImageAdjust: apilado LP+WB+EV, monotonicidad, denoise, clamps.

add_executable(test_imadjust test_imadjust/main.cpp
  ../src/common/ImageAdjust.h
  ../src/common/ImageAdjust.cpp)

target_include_directories(test_imadjust PRIVATE ../src)
target_compile_definitions(test_imadjust PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_imadjust PRIVATE ${OpenCV_LIBS})

add_test(NAME test_imadjust COMMAND test_imadjust)

# Filtro de píxeles calientes sobre el mosaico Bayer: detección, corrección,
# invariancia de fase CFA, aislamiento y guardas sobre mosaicos no Bayer.

add_executable(test_bayerhot test_bayerhot/main.cpp
  ../src/raw/BayerHotPixels.h
  ../src/raw/BayerHotPixels.cpp)

target_include_directories(test_bayerhot PRIVATE ../src)
target_compile_definitions(test_bayerhot PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_bayerhot PRIVATE ${OpenCV_LIBS})

add_test(NAME test_bayerhot COMMAND test_bayerhot)

# Detector TEMPORAL de píxeles calientes sobre vídeo: separa los quemados fijos
# de una estrella que se mueve, con un IVideoReader sintético.

add_executable(test_hotpixels test_hotpixels/main.cpp
  ../src/processing/HotPixelMap.h
  ../src/processing/HotPixelMap.cpp
  ../src/common/ImageAdjust.h
  ../src/common/ImageAdjust.cpp
  ../src/video/IVideoReader.h)

target_include_directories(test_hotpixels PRIVATE ../src)
target_compile_definitions(test_hotpixels PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_hotpixels PRIVATE Qt6::Core ${OpenCV_LIBS})

add_test(NAME test_hotpixels COMMAND test_hotpixels)

# Verificación local opcional con CR2 reales de un eclipse (testdata/eclipse/,
# excluido del repo). Si la carpeta no existe, el test se omite.

add_executable(test_eclipse test_eclipse/main.cpp
  ../src/stills/PhotoSequenceReader.h
  ../src/stills/PhotoSequenceReader.cpp
  ../src/raw/RawDecoder.h
  ../src/raw/RawDecoder.cpp
  ../src/raw/BayerHotPixels.h
  ../src/raw/BayerHotPixels.cpp
  ../src/raw/RawExifReader.h
  ../src/raw/RawExifReader.cpp
  ../src/motion/TrackStatus.h
  ../src/motion/Kalman.h
  ../src/motion/Kalman.cpp
  ../src/motion/MotionModel.h
  ../src/motion/MotionModel.cpp
  ../src/tracking/CircleEstimator.h
  ../src/tracking/CircleEstimator.cpp
  ../src/tracking/DiscArcFit.h
  ../src/tracking/DiscArcFit.cpp
  ../src/tracking/DiscDetection.h
  ../src/tracking/EccDiscDetector.h
  ../src/tracking/EccDiscDetector.cpp
  ../src/tracking/FeatureDiscDetector.h
  ../src/tracking/FeatureDiscDetector.cpp
  ../src/tracking/IDiscDetector.h
  ../src/tracking/LimbScorer.h
  ../src/tracking/LimbScorer.cpp
  ../src/tracking/DiscFusion.h
  ../src/tracking/DiscFusion.cpp
  ../src/tracking/TemplateDiscDetector.h
  ../src/tracking/TemplateDiscDetector.cpp
  ../src/tracking/CentroidDiscDetector.h
  ../src/tracking/CentroidDiscDetector.cpp
  ../src/tracking/KnownRadiusDiscDetector.h
  ../src/tracking/KnownRadiusDiscDetector.cpp
  ../src/tracking/PhaseCorrelationDiscDetector.h
  ../src/tracking/PhaseCorrelationDiscDetector.cpp
  ../src/tracking/ArcBlobDiscDetector.h
  ../src/tracking/ArcBlobDiscDetector.cpp
  ../src/tracking/DiscTracker.h
  ../src/tracking/DiscTracker.cpp)

target_include_directories(test_eclipse PRIVATE ../src)
target_compile_definitions(test_eclipse PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
  TESTDATA_ECLIPSE="${CMAKE_SOURCE_DIR}/testdata/eclipse")
target_link_libraries(test_eclipse PRIVATE ${OpenCV_LIBS} libraw)

if(EXISTS ${CMAKE_SOURCE_DIR}/testdata/eclipse)
  add_test(NAME test_eclipse COMMAND test_eclipse)
endif()

# Regresión del centrado en la exportación de fotos: JPG y MP4 con fotogramas
# intermedios deben dejar el disco en el centro del frame.

add_executable(test_export test_export/main.cpp
  ../src/common/Frame.h
  ../src/common/AppLog.h
  ../src/common/ImageAdjust.h
  ../src/common/ImageAdjust.cpp
  ../src/video/IVideoReader.h
  ../src/video/FFmpegVideoReader.h
  ../src/video/FFmpegVideoReader.cpp
  ../src/video/IVideoWriter.h
  ../src/video/FFmpegVideoWriter.h
  ../src/video/FFmpegVideoWriter.cpp
  ../src/processing/BorderHandler.h
  ../src/processing/BorderHandler.cpp
  ../src/stills/PhotoExportWorker.h
  ../src/stills/PhotoExportWorker.cpp
  ../src/stills/VideoExportWorker.h
  ../src/stills/VideoExportWorker.cpp
  ../src/stills/PhotoSequenceReader.h
  ../src/stills/PhotoSequenceReader.cpp
  ../src/raw/RawDecoder.h
  ../src/raw/RawDecoder.cpp
  ../src/raw/BayerHotPixels.h
  ../src/raw/BayerHotPixels.cpp
  ../src/raw/RawExifReader.h
  ../src/raw/RawExifReader.cpp
  ../src/motion/TrackStatus.h
  ../src/motion/MotionModel.h
  ../src/motion/Kalman.h
  ../src/tracking/DiscTracker.h)

target_include_directories(test_export PRIVATE ../src)
target_compile_definitions(test_export PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_export PRIVATE
  Qt6::Core
  ${OpenCV_LIBS}
  libraw
  FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale)

add_test(NAME test_export COMMAND test_export)

# Proyectos .atracker: serialización JSON del trabajo (Fotos y Vídeo).

add_executable(test_project test_project/main.cpp
  ../src/stills/PhotoProject.h
  ../src/stills/PhotoProject.cpp)

target_include_directories(test_project PRIVATE ../src)
target_compile_definitions(test_project PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_project PRIVATE Qt6::Core ${OpenCV_LIBS})

add_test(NAME test_project COMMAND test_project)

# Nomenclatura de exportación: timestamp + nombre del origen, saneado.

add_executable(test_exportnaming test_exportnaming/main.cpp
  ../src/common/ExportNaming.h
  ../src/common/ExportNaming.cpp)

target_include_directories(test_exportnaming PRIVATE ../src)
target_compile_definitions(test_exportnaming PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_exportnaming PRIVATE Qt6::Core ${OpenCV_LIBS})

add_test(NAME test_exportnaming COMMAND test_exportnaming)

# Worker de ajustes: aplica fuera del hilo principal y coalesce peticiones.

add_executable(test_adjustworker test_adjustworker/main.cpp
  ../src/common/ImageAdjust.h
  ../src/common/ImageAdjust.cpp
  ../src/processing/AdjustWorker.h
  ../src/processing/AdjustWorker.cpp)

target_include_directories(test_adjustworker PRIVATE ../src)
target_compile_definitions(test_adjustworker PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_adjustworker PRIVATE Qt6::Core ${OpenCV_LIBS})

add_test(NAME test_adjustworker COMMAND test_adjustworker)

# Reparto de atajos: con modificador = QShortcut global; tecla pelada = por foco.
# Regresión de los sliders del dock, a los que las flechas globales robaban.

add_executable(test_shortcuts test_shortcuts/main.cpp
  ../src/ui/shortcuts/ShortcutManager.h
  ../src/ui/shortcuts/ShortcutManager.cpp)

target_include_directories(test_shortcuts PRIVATE ../src)
target_compile_definitions(test_shortcuts PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_shortcuts PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Usa QGuiApplication (QKeySequence es de QtGui): necesita plataforma gráfica,
# pero offscreen y sin ventana visible. Qt6_DIR apunta a <qt>/lib/cmake/Qt6,
# así que los plugins (incluido platforms/qoffscreen.dll) están tres niveles
# arriba.
add_test(NAME test_shortcuts COMMAND test_shortcuts)
if(Qt6_DIR)
  get_filename_component(_qt_plugins "${Qt6_DIR}/../../.." ABSOLUTE)
  set_tests_properties(test_shortcuts PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_qt_plugins}/plugins")
endif()

# Tests sintéticos de los detectores de disco (KnownRadius, Centroid,
# PhaseCorrelation) y de la fusión por soporte radial.

add_executable(test_detectors test_detectors/main.cpp
  ../src/motion/TrackStatus.h
  ../src/tracking/DiscDetection.h
  ../src/tracking/EccDiscDetector.h
  ../src/tracking/EccDiscDetector.cpp
  ../src/tracking/FeatureDiscDetector.h
  ../src/tracking/FeatureDiscDetector.cpp
  ../src/tracking/IDiscDetector.h
  ../src/tracking/DiscTrackerParams.h
  ../src/tracking/DiscArcFit.h
  ../src/tracking/DiscArcFit.cpp
  ../src/tracking/LimbScorer.h
  ../src/tracking/LimbScorer.cpp
  ../src/tracking/DiscFusion.h
  ../src/tracking/DiscFusion.cpp
  ../src/tracking/KnownRadiusDiscDetector.h
  ../src/tracking/KnownRadiusDiscDetector.cpp
  ../src/tracking/CentroidDiscDetector.h
  ../src/tracking/CentroidDiscDetector.cpp
  ../src/tracking/PhaseCorrelationDiscDetector.h
  ../src/tracking/PhaseCorrelationDiscDetector.cpp)

target_include_directories(test_detectors PRIVATE ../src)
target_compile_definitions(test_detectors PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_detectors PRIVATE ${OpenCV_LIBS})

add_test(NAME test_detectors COMMAND test_detectors)
