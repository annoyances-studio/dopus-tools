# animview_core: pure, Opus-free, GDI-free logic.
# Depends only on libwebp. This is the part we unit-test headlessly.

add_library(animview_core STATIC
  src/webp_probe.cpp
  src/webp_version.cpp
  src/decoder.cpp
  src/anim_source.cpp
  src/anim_player.cpp
  src/gif_player.cpp
)

target_include_directories(animview_core PUBLIC include)

# webp/webpdemux = WebP (WebPAnimDecoder); windowscodecs = WIC (GIF); ole32 = COM.
target_link_libraries(animview_core PUBLIC webp webpdemux windowscodecs ole32)

target_compile_definitions(animview_core PUBLIC UNICODE _UNICODE)
