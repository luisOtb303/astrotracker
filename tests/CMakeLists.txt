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