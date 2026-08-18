# FindFFmpeg.cmake — localiza el dev-package de FFmpeg (gyan.dev) y expone
# targets importados FFmpeg::avformat, FFmpeg::avcodec, FFmpeg::avutil, FFmpeg::swscale.
#
# Uso: -DFFMPEG_ROOT=C:/ffmpeg  (prefijo con include/ y lib/)
# Debe llamarse ANTES de find_package(Qt6) para que Qt no lo sombree con su
# propio FindFFmpeg.cmake.

if(NOT DEFINED FFMPEG_ROOT)
  set(FFMPEG_ROOT "C:/ffmpeg" CACHE PATH "Prefijo de instalación de FFmpeg")
endif()

find_path(FFmpeg_INCLUDE_DIR
  NAMES libavformat/avformat.h
  HINTS ${FFMPEG_ROOT}/include
  NO_DEFAULT_PATH)

foreach(comp avformat avcodec avutil swscale)
  find_library(FFmpeg_${comp}_LIBRARY
    NAMES ${comp}
    HINTS ${FFMPEG_ROOT}/lib
    NO_DEFAULT_PATH)
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS
    FFmpeg_INCLUDE_DIR
    FFmpeg_avformat_LIBRARY
    FFmpeg_avcodec_LIBRARY
    FFmpeg_avutil_LIBRARY
    FFmpeg_swscale_LIBRARY)

if(FFmpeg_FOUND AND NOT TARGET FFmpeg::avformat)
  set(FFmpeg_INCLUDE_DIRS ${FFmpeg_INCLUDE_DIR})
  set(FFmpeg_LIBRARIES
    ${FFmpeg_avformat_LIBRARY}
    ${FFmpeg_avcodec_LIBRARY}
    ${FFmpeg_avutil_LIBRARY}
    ${FFmpeg_swscale_LIBRARY})

  foreach(comp avformat avcodec avutil swscale)
    file(GLOB _ffmpeg_dll "${FFMPEG_ROOT}/bin/${comp}-*.dll")
    add_library(FFmpeg::${comp} SHARED IMPORTED)
    set_target_properties(FFmpeg::${comp} PROPERTIES
      IMPORTED_LOCATION "${_ffmpeg_dll}"
      IMPORTED_IMPLIB "${FFmpeg_${comp}_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_INCLUDE_DIR}")
    if(NOT _ffmpeg_dll)
      message(WARNING "No se encontró la DLL de FFmpeg '${comp}-*.dll' en ${FFMPEG_ROOT}/bin")
    endif()
  endforeach()
endif()