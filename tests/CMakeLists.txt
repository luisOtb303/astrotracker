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

# Test del lector de secuencias de fotos (orden natural, formatos, tamaños).

add_executable(test_stills test_stills/main.cpp
  ../src/stills/PhotoSequenceReader.h
  ../src/stills/PhotoSequenceReader.cpp)

target_include_directories(test_stills PRIVATE ../src)
target_compile_definitions(test_stills PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_stills PRIVATE ${OpenCV_LIBS})

add_test(NAME test_stills COMMAND test_stills)