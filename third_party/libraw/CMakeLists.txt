# Wrapper CMake de LibRaw (0.22.2).
# Réplica del proyecto oficial buildfiles/libraw.vcxproj: exactamente los mismos
# .cpp (se excluyen los *_ph.cpp de la nueva API de fases), mismos defines y
# avisos desactivados. Licencia LGPL-2.1/CDDL, compatible GPLv3.
cmake_minimum_required(VERSION 3.21)

project(libraw LANGUAGES CXX)

file(GLOB RAW_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp)
list(FILTER RAW_SOURCES EXCLUDE REGEX ".*_ph\\.cpp$")

add_library(libraw STATIC ${RAW_SOURCES})
target_include_directories(libraw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(libraw PROPERTIES POSITION_INDEPENDENT_CODE ON)

# PUBLIC para propagar LIBRAW_BUILDLIB a los consumidores: el header usa
# DllDef/__declspec(dllimport) cuando no se define, lo que rompería el enlazado
# contra una lib estática. Con LIBRAW_BUILDLIB, DllDef queda vacío.
if(WIN32)
  target_compile_definitions(libraw PUBLIC
    _WINDOWS UNICODE _UNICODE WIN32
    _ENABLE_EXTENDED_ALIGNED_STORAGE WIN64 LIBRAW_BUILDLIB)
endif()

target_compile_options(libraw PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W0>)