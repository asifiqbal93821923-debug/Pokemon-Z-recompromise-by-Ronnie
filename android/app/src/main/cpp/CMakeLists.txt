cmake_minimum_required(VERSION 3.22.1)
project(za_android_sdl3_probe C CXX)
add_subdirectory(SDL EXCLUDE_FROM_ALL)

add_library(za_probe SHARED native_probe.c android_storage_probe.c)
target_compile_features(za_probe PRIVATE c_std_11)
target_compile_options(za_probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_probe PRIVATE log android)

add_library(za_android_input STATIC za_android_input.c)
target_compile_features(za_android_input PUBLIC c_std_11)
target_compile_options(za_android_input PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_android_input PUBLIC SDL3::SDL3)

# The pinned upstream runtime platform.h is patched only in the CI checkout
# so Android does not include the glibc-only Linux compatibility header.
set(ZA_UPSTREAM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../vendor/pokeza-upstream")
set(ZA_UPSTREAM_RUNTIME "${ZA_UPSTREAM_ROOT}/runtime")

add_library(za_android_platform_backend OBJECT
    android_platform.c
    android_platform_link_probe.c
)
target_include_directories(za_android_platform_backend PRIVATE
    "${ZA_UPSTREAM_RUNTIME}"
)
target_compile_features(za_android_platform_backend PRIVATE c_std_11)
target_compile_options(za_android_platform_backend PRIVATE -Wall -Wextra -Werror)
target_link_libraries(za_android_platform_backend PRIVATE SDL3::SDL3 log android dl)

# Compile the real upstream runtime core on Android ARM64. This intentionally
# excludes the desktop platform sources and GPU/UI backends; the latter need
# their separate Android Vulkan + dependency integration stage.
file(GLOB ZA_RUNTIME_ROOT_SOURCES CONFIGURE_DEPENDS
    "${ZA_UPSTREAM_RUNTIME}/*.c"
    "${ZA_UPSTREAM_RUNTIME}/*.cpp"
)
file(GLOB ZA_RUNTIME_HLE_SOURCES CONFIGURE_DEPENDS
    "${ZA_UPSTREAM_RUNTIME}/hle/*.c"
)
file(GLOB ZA_RUNTIME_SERVICE_SOURCES CONFIGURE_DEPENDS
    "${ZA_UPSTREAM_RUNTIME}/services/*.c"
)
list(FILTER ZA_RUNTIME_ROOT_SOURCES EXCLUDE REGEX "/platform/(linux|macos|windows)\\.c$")
set(ZA_RUNTIME_CORE_SOURCES
    ${ZA_RUNTIME_ROOT_SOURCES}
    ${ZA_RUNTIME_HLE_SOURCES}
    ${ZA_RUNTIME_SERVICE_SOURCES}
)
add_library(za_runtime_core_compilecheck STATIC ${ZA_RUNTIME_CORE_SOURCES})
target_include_directories(za_runtime_core_compilecheck PRIVATE
    "${ZA_UPSTREAM_RUNTIME}"
    "${ZA_UPSTREAM_RUNTIME}/nvn"
    "${ZA_UPSTREAM_ROOT}/third_party/imgui"
    "${ZA_UPSTREAM_ROOT}/third_party/imgui/backends"
    "${ZA_UPSTREAM_ROOT}/third_party/ooz"
    "${ZA_UPSTREAM_ROOT}/third_party/ryujinx"
)
target_compile_features(za_runtime_core_compilecheck PRIVATE
    c_std_11
    cxx_std_17
)
target_compile_options(za_runtime_core_compilecheck PRIVATE
    -Wall -Wextra -Wno-unused-function -Wno-format -fno-strict-aliasing
)
target_link_libraries(za_runtime_core_compilecheck PRIVATE SDL3::SDL3 log android dl)

# This is a compile-check archive only; it is not linked into the diagnostic
# APK as an executable game runtime. It catches Android/Bionic source errors
# early without pretending generated game code or renderer are ready.

add_library(main SHARED
    sdl_probe.c
    vulkan_probe.c
    guest_memory_probe.c
    guest_write_tracking_probe.c
    android_platform_probe.c
)
target_include_directories(main PRIVATE "${ZA_UPSTREAM_RUNTIME}")
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE -Wall -Wextra -Werror)
target_link_libraries(main PRIVATE
    SDL3::SDL3
    za_android_input
    za_android_platform_backend
    vulkan
    log
    android
    dl
)
