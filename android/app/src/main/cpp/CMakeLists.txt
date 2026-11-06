cmake_minimum_required(VERSION 3.22.1)
project(za_android_sdl3_probe C)
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# Isolated JNI probe: this library can be loaded by the Java host without
# starting SDL, helping distinguish Android/JNI packaging from SDL startup.
add_library(za_probe SHARED native_probe.c)
target_compile_features(za_probe PRIVATE c_std_11)
target_compile_options(za_probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_probe PRIVATE log)

add_library(za_android_input STATIC za_android_input.c)
target_compile_features(za_android_input PUBLIC c_std_11)
target_compile_options(za_android_input PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_android_input PUBLIC SDL3::SDL3)

add_library(main SHARED sdl_probe.c vulkan_probe.c)
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE -Wall -Wextra -Werror)
target_link_libraries(main PRIVATE SDL3::SDL3 za_android_input vulkan log)
