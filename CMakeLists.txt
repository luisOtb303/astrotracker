cmake_minimum_required(VERSION 3.21)
project(AstroTracker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swscale)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV REQUIRED)

add_executable(astrotracker
  src/app/main.cpp
  src/common/Frame.h
  src/video/IVideoReader.h
  src/video/FFmpegVideoReader.h
  src/video/FFmpegVideoReader.cpp
  src/ui/VideoView.h
  src/ui/VideoView.cpp
  src/ui/MainWindow.h
  src/ui/MainWindow.cpp
)

target_include_directories(astrotracker PRIVATE src)
target_compile_definitions(astrotracker PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(astrotracker PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets
  ${OpenCV_LIBS}
  FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale
)

# Deploy Qt runtime (DLLs) junto al ejecutable
add_custom_command(TARGET astrotracker POST_BUILD
  COMMAND $<TARGET_FILE_DIR:Qt6::Core>/windeployqt.exe --no-translations --no-opengl-sw --release $<TARGET_FILE:astrotracker>
  COMMENT "Desplegando runtime Qt")

# Copiar DLLs de FFmpeg junto al ejecutable
file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
add_custom_command(TARGET astrotracker POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_DLLS} $<TARGET_FILE_DIR:astrotracker>
  COMMENT "Copiando DLLs de FFmpeg")

# Copiar DLL de OpenCV junto al ejecutable
file(GLOB OPENCV_DLLS "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll")
add_custom_command(TARGET astrotracker POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCV_DLLS} $<TARGET_FILE_DIR:astrotracker>
  COMMENT "Copiando DLLs de OpenCV")

enable_testing()
add_subdirectory(tests)