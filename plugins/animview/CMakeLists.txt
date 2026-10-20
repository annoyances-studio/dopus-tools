cmake_minimum_required(VERSION 3.21)

project(animview LANGUAGES CXX VERSION 0.1.0)

if(NOT WIN32)
  message(FATAL_ERROR "animview is a Directory Opus viewer plugin and only builds on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ANIMVIEW_BUILD_TESTS "Build the animview_core unit tests" ON)

# Path to the GPSoftware Opus Plugin SDK (headers/ lives under here).
# The SDK is not redistributed in this repo; override if yours is elsewhere.
set(OPUS_SDK_DIR "C:/Code/dopus-sdk/opus_sdk" CACHE PATH "Opus Plugin SDK root")

# --- libwebp (self-contained; built from source, no vcpkg needed) --------------
# We need the decoder (webp) and the demux/animation API (webpdemux => WebPAnimDecoder).
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)   # static-link libwebp into animview.dll
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP      OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP      OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP   OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP   OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP      OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO   OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX    OFF CACHE BOOL "" FORCE)
# libwebpmux (WebPAnimEncoder) is ON: the tests synthesize an animated WebP
# in-memory as a decode fixture. The core/plugin do not depend on it.
set(WEBP_BUILD_LIBWEBPMUX ON  CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS     OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  libwebp
  GIT_REPOSITORY https://github.com/webmproject/libwebp.git
  GIT_TAG        v1.4.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libwebp)

add_subdirectory(core)

# The plugin DLL needs the GPSoftware SDK (not redistributable). When it's absent
# — e.g. on CI — core + tests still build, which is the logic worth gating.
if(EXISTS "${OPUS_SDK_DIR}/headers/viewer plugins.h")
  add_subdirectory(opus)
else()
  message(STATUS "animview: Opus SDK not at '${OPUS_SDK_DIR}' — skipping the plugin DLL "
                 "(core + tests still build). Set -DOPUS_SDK_DIR to build the DLL.")
endif()

if(ANIMVIEW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
