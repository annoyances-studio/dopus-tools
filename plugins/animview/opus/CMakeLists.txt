# animview (the Opus-facing DLL): thin shell around animview_core.
# Exports the DVP_* functions Opus calls; owns the window + GDI (later increments).

if(NOT EXISTS "${OPUS_SDK_DIR}/headers/viewer plugins.h")
  message(FATAL_ERROR
    "Opus Plugin SDK not found at OPUS_SDK_DIR='${OPUS_SDK_DIR}'. "
    "Set -DOPUS_SDK_DIR=<path to opus_sdk> (the folder containing headers/).")
endif()

add_library(animview SHARED
  src/dllmain.cpp
  src/exports.cpp
  src/file_io.cpp
  src/viewer.cpp
)

set_target_properties(animview PROPERTIES
  OUTPUT_NAME "animview"
  PREFIX ""      # no "lib" prefix
  SUFFIX ".dll"
)

target_include_directories(animview PRIVATE
  "${OPUS_SDK_DIR}/headers"
  include
)

target_link_libraries(animview PRIVATE animview_core msimg32)  # msimg32 = AlphaBlend

target_compile_definitions(animview PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
