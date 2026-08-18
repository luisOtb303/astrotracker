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
  ../src/stabilization/Stabilizer.cpp)

target_include_directories(test_stabilizer PRIVATE ../src)
target_compile_definitions(test_stabilizer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(test_stabilizer PRIVATE ${OpenCV_LIBS})

add_test(NAME test_stabilizer COMMAND test_stabilizer)