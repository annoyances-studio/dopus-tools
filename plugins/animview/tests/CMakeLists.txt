# Headless unit tests for animview_core. No test framework — a plain executable
# that returns non-zero on failure, registered with CTest. Keep it dependency-light.

add_executable(animview_core_tests test_webp_probe.cpp)
target_link_libraries(animview_core_tests PRIVATE animview_core)
add_test(NAME animview_core_tests COMMAND animview_core_tests)

# Decode test: synthesizes an animated WebP with WebPAnimEncoder (libwebpmux),
# decodes the first frame through the core, checks dimensions + pixels.
add_executable(animview_decode_tests test_decoder.cpp)
target_link_libraries(animview_decode_tests PRIVATE animview_core libwebpmux webp)
add_test(NAME animview_decode_tests COMMAND animview_decode_tests)

# Player test: frame stepping + looping over a synthesized animation.
add_executable(animview_anim_tests test_anim_player.cpp)
target_link_libraries(animview_anim_tests PRIVATE animview_core libwebpmux webp)
add_test(NAME animview_anim_tests COMMAND animview_anim_tests)

# GIF test: WIC decode + disposal compositing + looping over a synthesized GIF.
add_executable(animview_gif_tests test_gif_player.cpp)
target_link_libraries(animview_gif_tests PRIVATE animview_core)
add_test(NAME animview_gif_tests COMMAND animview_gif_tests)
