cmake_minimum_required(VERSION 3.22.1)
project(za_android_sdl3_probe C)
add_subdirectory(SDL EXCLUDE_FROM_ALL)

add_library(za_probe SHARED native_probe.c android_storage_probe.c)
target_compile_features(za_probe PRIVATE c_std_11)
target_compile_options(za_probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_probe PRIVATE log android)

add_library(za_android_input STATIC za_android_input.c)
target_compile_features(za_android_input PUBLIC c_std_11)
target_compile_options(za_android_input PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_android_input PUBLIC SDL3::SDL3)

# Link the Android platform backend into the SDL native library, not just compile it.
add_library(za_android_platform_compilecheck OBJECT
    android_platform.c
    android_platform_link_probe.c
)
target_include_directories(za_android_platform_compilecheck PRIVATE upstream/runtime)
target_compile_features(za_android_platform_compilecheck PRIVATE c_std_11)
target_compile_options(za_android_platform_compilecheck PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_android_platform_compilecheck PRIVATE SDL3::SDL3 log android dl)

add_library(main SHARED
    sdl_probe.c
    vulkan_probe.c
    guest_memory_probe.c
    guest_write_tracking_probe.c
    android_platform_probe.c
)
target_include_directories(main PRIVATE upstream/runtime)
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE -Wall -Wextra -Werror)
target_link_libraries(main PRIVATE
    SDL3::SDL3
    za_android_input
    za_android_platform_compilecheck
    vulkan
    log
    android
    dl
)
