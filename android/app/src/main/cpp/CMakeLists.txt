cmake_minimum_required(VERSION 3.22.1)
project(za_android_sdl3_probe C)

# SDL3 is checked out at a pinned upstream release by the CI workflow.
add_subdirectory(SDL EXCLUDE_FROM_ALL)

add_library(main SHARED sdl_probe.c)
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE -Wall -Wextra -Werror)
target_link_libraries(main PRIVATE SDL3::SDL3 log)
