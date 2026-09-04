cmake_minimum_required(VERSION 3.21)
project(AstroTracker VERSION 0.4.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Icono del .exe: se genera app.rc a partir de la plantilla con la ruta absoluta
# del .ico (multi-tamaño) para el compilador de recursos de MSVC.
set(APP_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/favicon/app.ico")
string(REPLACE "\\" "/" APP_ICON_PATH "${APP_ICON_PATH}")
configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swscale)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV REQUIRED)
add_subdirectory(third_party/libraw)

# exiv2 vendido en-tree (lectura EXIF de JPEG/PNG/TIFF). Se compila como
# librería estática con el mínimo de sub-sistemas (sin XMP/PNG/BMFF/vídeo/NLS)
# para no arrastrar dependencias externas (expat, zlib, brotli, curl, inih).
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(EXIV2_ENABLE_XMP OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_EXTERNAL_XMP OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_PNG OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_NLS OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_LENSDATA OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_BMFF OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_BROTLI OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_VIDEO OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_WEBREADY OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_CURL OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_INIH OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_EXIV2_COMMAND OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_FUZZ_TESTS OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EXIV2_TEAM_PACKAGING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/exiv2)
# Los cabeceras generadas (exv_conf.h, exiv2lib_export.h) se generan en
# ${CMAKE_BINARY_DIR} (root del build); se propagan a los consumidores.
target_include_directories(exiv2lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>")

add_executable(astrotracker WIN32
  src/app/main.cpp
  src/common/Frame.h
  src/common/CircleF.h
  src/common/PhotoExifInfo.h
  src/video/IVideoReader.h
  src/video/FFmpegVideoReader.h
  src/video/FFmpegVideoReader.cpp
  src/ui/VideoView.h
  src/ui/VideoView.cpp
  src/ui/MainWindow.h
  src/ui/MainWindow.cpp
  src/ui/AboutDialog.h
  src/ui/AboutDialog.cpp
  src/ui/theme/ThemeManager.h
  src/ui/theme/ThemeManager.cpp
  src/ui/panels/InfoPanel.h
  src/ui/panels/InfoPanel.cpp
  src/ui/timeline/TimelineWidget.h
  src/ui/timeline/TimelineWidget.cpp
  src/ui/shortcuts/ShortcutManager.h
  src/ui/shortcuts/ShortcutManager.cpp
  resources/resources.qrc
  ${CMAKE_CURRENT_BINARY_DIR}/app.rc
  src/tracking/ITracker.h
  src/tracking/TemplateTracker.h
  src/tracking/TemplateTracker.cpp
  src/tracking/CentroidTracker.h
  src/tracking/CentroidTracker.cpp
  src/tracking/CircleEstimator.h
  src/tracking/CircleEstimator.cpp
  src/tracking/DiscDetection.h
  src/tracking/EccDiscDetector.h
  src/tracking/EccDiscDetector.cpp
  src/tracking/FeatureDiscDetector.h
  src/tracking/FeatureDiscDetector.cpp
  src/tracking/IDiscDetector.h
  src/tracking/LimbScorer.h
  src/tracking/LimbScorer.cpp
  src/tracking/DiscFusion.h
  src/tracking/DiscFusion.cpp
  src/tracking/TemplateDiscDetector.h
  src/tracking/TemplateDiscDetector.cpp
  src/tracking/CentroidDiscDetector.h
  src/tracking/CentroidDiscDetector.cpp
  src/tracking/KnownRadiusDiscDetector.h
  src/tracking/KnownRadiusDiscDetector.cpp
  src/tracking/PhaseCorrelationDiscDetector.h
  src/tracking/PhaseCorrelationDiscDetector.cpp
  src/tracking/ArcBlobDiscDetector.h
  src/tracking/ArcBlobDiscDetector.cpp
  src/tracking/TrackingProfile.h
  src/tracking/DiscTrackerParams.h
  src/tracking/DiscTracker.h
  src/tracking/DiscTracker.cpp
  src/tracking/DiscArcFit.h
  src/tracking/DiscArcFit.cpp
  src/motion/TrackStatus.h
  src/motion/Kalman.h
  src/motion/Kalman.cpp
  src/motion/MotionModel.h
  src/motion/MotionModel.cpp
  src/stabilization/TargetPosition.h
  src/stabilization/SmoothingFilter.h
  src/stabilization/SmoothingFilter.cpp
  src/stabilization/Stabilizer.h
  src/stabilization/Stabilizer.cpp
  src/video/IVideoWriter.h
  src/video/FFmpegVideoWriter.h
  src/video/FFmpegVideoWriter.cpp
  src/processing/BorderHandler.h
  src/processing/BorderHandler.cpp
  src/processing/FrameTransformer.h
  src/common/AppLog.h
  src/processing/Pipeline.h
  src/processing/Pipeline.cpp
  src/export/ExportJob.h
  src/export/ExportJob.cpp
  src/export/PipelineWorker.h
  src/stills/PhotoSequenceReader.h
  src/stills/PhotoSequenceReader.cpp
  src/stills/PhotoTrackWorker.h
  src/stills/PhotoFrameLoader.h
  src/stills/PhotoFrameLoader.cpp
  src/stills/PhotoExportWorker.h
  src/stills/PhotoExportWorker.cpp
  src/stills/PhotoProject.h
  src/stills/PhotoProject.cpp
  src/stills/ExifReader.h
  src/stills/ExifReader.cpp
  src/stills/VideoExportWorker.h
  src/stills/VideoExportWorker.cpp
  src/raw/RawDecoder.h
  src/raw/RawDecoder.cpp
  src/raw/RawExifReader.h
  src/raw/RawExifReader.cpp
  src/ui/PhotoPanel.h
  src/ui/PhotoPanel.cpp
  src/ui/PhotoFilmstrip.h
  src/ui/PhotoFilmstrip.cpp
)

target_include_directories(astrotracker PRIVATE src)

# Revisión git (commit corto) para el diálogo "Acerca de"; fallback si no hay git.
execute_process(COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE ASTROTRACKER_GIT_REV
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT ASTROTRACKER_GIT_REV)
  set(ASTROTRACKER_GIT_REV "unknown")
endif()

# La versión vive en `project(... VERSION)` (semver); se propaga al Acerca de y a
# la app. La compilación usa la configuración activa ($<CONFIG>).
target_compile_definitions(astrotracker PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN
  ASTROTRACKER_VERSION="${PROJECT_VERSION}"
  ASTROTRACKER_BUILD_TYPE="$<CONFIG>"
  ASTROTRACKER_GIT_REV="${ASTROTRACKER_GIT_REV}"
  ASTROTRACKER_HAS_EXIV2)

target_link_libraries(astrotracker PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets
  ${OpenCV_LIBS}
  libraw
  FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale
)
target_link_libraries(astrotracker PRIVATE exiv2lib)
target_compile_definitions(astrotracker PRIVATE ASTROTRACKER_HAS_EXIV2)

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

# ---- Empaquetado (CPack) ----
# Genera con un solo comando (cmake --build build --config Release && cpack -C
# Release): ZIP portable (libarchive integrado, sin herramientas externas) e
# instalador EXE (requiere NSIS). Contenido: exe + DLLs (Qt/FFmpeg/OpenCV y
# plugins) + documentación + licencias.
install(TARGETS astrotracker RUNTIME DESTINATION .)
install(DIRECTORY "$<TARGET_FILE_DIR:astrotracker>/" DESTINATION .
  FILES_MATCHING PATTERN "*.dll"
  PATTERN "opencv_world*d.dll" EXCLUDE)
install(FILES README.md THIRD_PARTY_LICENSES/index.md DESTINATION .)
install(DIRECTORY resources/licenses/ DESTINATION licenses)

set(CPACK_PACKAGE_NAME "AstroTracker")
set(CPACK_PACKAGE_VENDOR "AstroTracker Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Seguimiento y estabilización de vídeos astronómicos (Sol/Luna)")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "AstroTracker")
set(CPACK_PACKAGE_FILE_NAME "AstroTracker-${PROJECT_VERSION}-win64")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/resources/licenses/gpl-3.0.txt")
set(CPACK_GENERATOR "ZIP;NSIS")

# Instalador NSIS: instala en Program Files, crea el acceso del menú Inicio,
# registra la desinstalación en "Aplicaciones" y ofrece quitar la versión previa.
set(CPACK_PACKAGE_EXECUTABLES "astrotracker" "AstroTracker")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
set(CPACK_NSIS_DISPLAY_NAME "AstroTracker")
set(CPACK_NSIS_PACKAGE_NAME "AstroTracker")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/favicon/app.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/favicon/app.ico")
set(CPACK_NSIS_INSTALLED_ICON_NAME "astrotracker.exe")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)