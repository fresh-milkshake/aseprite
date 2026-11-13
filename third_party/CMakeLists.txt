# Aseprite
# Copyright (C) 2021-2025  Igara Studio S.A.
# Copyright (C) 2001-2018  David Capello

include_directories(.)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

if(NOT USE_SHARED_ZLIB)
  set(SKIP_INSTALL_ALL on)
  # Don't build zlib tests
  set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Enable Zlib Examples")
  set(ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/zlib" CACHE INTERNAL "")
  add_subdirectory(zlib)
  add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

if(NOT USE_SHARED_LIBPNG)
  set(SKIP_INSTALL_ALL ON)
  # We only need the static version of libpng
  set(PNG_SHARED OFF CACHE BOOL "Build shared lib")
  set(PNG_STATIC ON CACHE BOOL "Build static lib")
  set(PNG_TESTS OFF CACHE BOOL "Build libpng tests")
  add_subdirectory(libpng)
endif()

if(NOT USE_SHARED_GIFLIB)
  set(GIFLIB_UTILS OFF CACHE BOOL "Build giflib utils")
  add_subdirectory(giflib)
endif()

if(NOT USE_SHARED_TINYXML)
  set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "Build tests for tinyxml2")
  add_subdirectory(tinyxml2)
endif()

if(NOT USE_SHARED_TINYEXIF)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "build as shared library")
  set(BUILD_STATIC_LIBS ON CACHE BOOL "build as static library")
  set(LINK_CRT_STATIC_LIBS OFF CACHE BOOL "link CRT static library")
  set(BUILD_DEMO OFF CACHE BOOL "build demo binary")
  add_subdirectory(TinyEXIF)
endif()

if(NOT USE_SHARED_PIXMAN)
  add_subdirectory(pixman-cmake)
endif()

add_subdirectory(simpleini)
add_subdirectory(fmt)

# Add cmark without tests
if(NOT USE_SHARED_CMARK)
  set(CMARK_PROGRAM OFF CACHE BOOL "Build cmark executable")
  set(CMARK_TESTS OFF CACHE BOOL "Build cmark tests and enable testing")
  add_subdirectory(cmark)
endif()

# JSON
add_subdirectory(json11)

# libarchive
set(HAVE_WCSCPY 1)
set(HAVE_WCSLEN 1)

set(ENABLE_WERROR OFF CACHE BOOL "Treat warnings as errors - default is ON for Debug, OFF otherwise.")
set(ENABLE_TEST OFF CACHE BOOL "Enable unit and regression tests")
set(ENABLE_COVERAGE OFF CACHE BOOL "Enable code coverage (GCC only, automatically sets ENABLE_TEST to ON)")
set(ENABLE_LZ4 OFF CACHE BOOL "Enable the use of the system LZ4 library if found")
set(ENABLE_LZO OFF CACHE BOOL "Enable the use of the system LZO library if found")
set(ENABLE_LZMA OFF CACHE BOOL "Enable the use of the system LZMA library if found")
set(ENABLE_ZSTD OFF CACHE BOOL "Enable the use of the system zstd library if found")
set(ENABLE_CNG OFF CACHE BOOL "Enable the use of CNG(Crypto Next Generation)")
set(ENABLE_BZip2 OFF CACHE BOOL "Enable the use of the system BZip2 library if found")
set(ENABLE_EXPAT OFF CACHE BOOL "Enable the use of the system EXPAT library if found")
set(ENABLE_LIBXML2 OFF CACHE BOOL "Enable the use of the system libxml2 library if found")
set(ENABLE_CAT OFF CACHE BOOL "Enable cat building")
set(ENABLE_TAR OFF CACHE BOOL "Enable tar building")
set(ENABLE_CPIO OFF CACHE BOOL "Enable cpio building")
set(ENABLE_LIBB2 OFF CACHE BOOL "Enable the use of the system LIBB2 library if found")
set(ENABLE_ICONV OFF CACHE BOOL "Enable iconv support")
set(WINDOWS_VERSION "WS08" CACHE STRING "Set Windows Vista as the target version for compiling libarchive (Windows only)" FORCE)
add_subdirectory(libarchive)
target_include_directories(archive_static INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libarchive/libarchive>)

# benchmark
if(ENABLE_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Enable testing of the benchmark library.")
  add_subdirectory(benchmark)
endif()

# tinyexpr
add_library(tinyexpr tinyexpr/tinyexpr.c)
target_include_directories(tinyexpr PUBLIC tinyexpr)

# cityhash
add_subdirectory(cityhash)

# lua
if(ENABLE_SCRIPTING)
  add_library(lua
    lua/lapi.c
    lua/lcode.c
    lua/lctype.c
    lua/ldebug.c
    lua/ldo.c
    lua/ldump.c
    lua/lfunc.c
    lua/lgc.c
    lua/llex.c
    lua/lmem.c
    lua/lobject.c
    lua/lopcodes.c
    lua/lparser.c
    lua/lstate.c
    lua/lstring.c
    lua/ltable.c
    lua/ltm.c
    lua/lundump.c
    lua/lvm.c
    lua/lzio.c
    lua/ltests.c)
  add_library(lauxlib lua/lauxlib.c)
  add_library(lualib
    lua/lbaselib.c
    lua/lcorolib.c
    lua/ldblib.c
    lua/linit.c
    lua/liolib.c
    lua/lmathlib.c
    lua/loadlib.c
    lua/loslib.c
    lua/lstrlib.c
    lua/ltablib.c
    lua/lutf8lib.c)

  if(WIN32)
    # LUA_USE_WINDOWS is defined in luaconf.h when we compile with _WIN32
    #target_compile_definitions(lua PUBLIC LUA_USE_WINDOWS=1)
  elseif(APPLE)
    target_compile_definitions(lua PUBLIC LUA_USE_MACOSX=1)
  elseif(UNIX)
    target_compile_definitions(lua PUBLIC LUA_USE_LINUX=1)
  endif()

  # Compile Lua C files as C++ to control errors with exceptions and
  # have stack unwinding (i.e. calling destructors correctly).
  file(GLOB all_lua_source_files lua/*.c)
  set_source_files_properties(${all_lua_source_files} PROPERTIES LANGUAGE CXX)

  target_compile_definitions(lua PUBLIC LUA_FLOORN2I=F2Ifloor)
  target_compile_definitions(lualib PRIVATE HAVE_SYSTEM)
  target_include_directories(lua PUBLIC lua)
  target_include_directories(lauxlib PUBLIC lua)
  target_include_directories(lualib PUBLIC lua)
  target_link_libraries(lauxlib lua)
  target_link_libraries(lualib lua)

  # ixwebsocket
  if(ENABLE_WEBSOCKET)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "Install IXWebSocket")
    add_subdirectory(IXWebSocket)
    target_include_directories(ixwebsocket PUBLIC)
  endif()

endif()

# qoi
add_library(qoi INTERFACE)
target_include_directories(qoi INTERFACE qoi)
