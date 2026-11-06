cmake_minimum_required(VERSION 3.22.1)
project(za_android_sdl3_probe C CXX ASM)
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

# The movie HLE requires Android FFmpeg prebuilts (avcodec/avformat/avutil/swscale).
# Keep it out of the core Bionic compile gate until that separate multimedia
# dependency is integrated; do not provide a fake movie implementation.
list(FILTER ZA_RUNTIME_HLE_SOURCES EXCLUDE REGEX "/movie\\.c$")
file(GLOB ZA_RUNTIME_SERVICE_SOURCES CONFIGURE_DEPENDS
    "${ZA_UPSTREAM_RUNTIME}/services/*.c"
)

if(ANDROID AND CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    # Keep the hand-written AArch64 context switch in a dedicated archive so
    # the assembler object is always included in the final link.
    add_library(za_android_fiber STATIC
        android_fiber_arm64.c
        android_fiber_arm64.S
    )
    target_include_directories(za_android_fiber PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    target_compile_features(za_android_fiber PUBLIC c_std_11)
    target_compile_options(za_android_fiber PRIVATE -Wall -Wextra -Werror)
endif()
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
    "${CMAKE_CURRENT_LIST_DIR}"
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
    za_android_fiber
    za_runtime_core_compilecheck
    vulkan
    log
    android
    dl
)


# Expand the Android compile gate to the actual upstream Vulkan/NVN renderer
# and ImGui backends. Generated shader include blobs are excluded until their
# build-time generators are wired into Android; this target is compile-only.
set(ZA_RUNTIME_NVN_SOURCES
    ${ZA_UPSTREAM_RUNTIME}/nvn/*.c
    ${ZA_UPSTREAM_RUNTIME}/nvn/maxwell/*.c
    ${ZA_UPSTREAM_RUNTIME}/nvn/vulkan/*.c
    ${ZA_UPSTREAM_RUNTIME}/nvn/vulkan/*.cpp
)
file(GLOB ZA_RUNTIME_NVN_SOURCES CONFIGURE_DEPENDS ${ZA_RUNTIME_NVN_SOURCES})
list(FILTER ZA_RUNTIME_NVN_SOURCES EXCLUDE REGEX "/(smaa_data|shader_prep)\\.c$")
list(FILTER ZA_RUNTIME_NVN_SOURCES EXCLUDE REGEX "/fsr2_shaders\\.cpp$")

set(ZA_IMGUI_ROOT "${ZA_UPSTREAM_ROOT}/third_party/imgui")
set(ZA_IMGUI_SOURCES
    "${ZA_IMGUI_ROOT}/imgui.cpp"
    "${ZA_IMGUI_ROOT}/imgui_draw.cpp"
    "${ZA_IMGUI_ROOT}/imgui_tables.cpp"
    "${ZA_IMGUI_ROOT}/imgui_widgets.cpp"
    "${ZA_IMGUI_ROOT}/backends/imgui_impl_sdl3.cpp"
    "${ZA_IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp"
)
add_library(za_runtime_graphics_compilecheck STATIC
    ${ZA_RUNTIME_NVN_SOURCES}
    ${ZA_IMGUI_SOURCES}
)
target_include_directories(za_runtime_graphics_compilecheck PRIVATE
    "${ZA_UPSTREAM_RUNTIME}"
    "${ZA_UPSTREAM_RUNTIME}/nvn"
    "${ZA_UPSTREAM_RUNTIME}/nvn/maxwell"
    "${ZA_UPSTREAM_RUNTIME}/nvn/vulkan"
    "${ZA_IMGUI_ROOT}"
    "${ZA_IMGUI_ROOT}/backends"
    "${ZA_UPSTREAM_ROOT}/third_party/fsr2"
    "${ZA_UPSTREAM_ROOT}/third_party/fsr2/vk"
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../vendor/vma/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../vendor/shaderc/libshaderc/include"
)
target_compile_features(za_runtime_graphics_compilecheck PRIVATE c_std_11 cxx_std_17)
target_compile_options(za_runtime_graphics_compilecheck PRIVATE
    -Wall -Wextra -Wno-unused-function -Wno-format -fno-strict-aliasing
)
target_link_libraries(za_runtime_graphics_compilecheck PRIVATE SDL3::SDL3 vulkan log android dl)


# Compile the portable upstream UI implementation as Android C++.
# The upstream Makefile compiles these .mm files as plain C++ on Linux;
# Apple-only Objective-C / Metal sections are guarded by __APPLE__.
file(GLOB ZA_RUNTIME_UI_SOURCES CONFIGURE_DEPENDS
    "${ZA_UPSTREAM_RUNTIME}/ui/*.mm"
)
if(NOT ZA_RUNTIME_UI_SOURCES)
    message(FATAL_ERROR "Pinned upstream runtime/ui/*.mm sources were not found; refusing an empty UI compile gate")
endif()
list(LENGTH ZA_RUNTIME_UI_SOURCES ZA_RUNTIME_UI_SOURCE_COUNT)
message(STATUS "ZA_ANDROID_UI_COMPILE_GATE source_count=${ZA_RUNTIME_UI_SOURCE_COUNT}")
set(CMAKE_VERBOSE_MAKEFILE ON)
set_source_files_properties(${ZA_RUNTIME_UI_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(za_runtime_ui_compilecheck STATIC ${ZA_RUNTIME_UI_SOURCES})
target_include_directories(za_runtime_ui_compilecheck PRIVATE
    "${ZA_UPSTREAM_RUNTIME}"
    "${ZA_UPSTREAM_RUNTIME}/ui"
    "${ZA_UPSTREAM_RUNTIME}/nvn"
    "${ZA_UPSTREAM_RUNTIME}/nvn/vulkan"
    "${ZA_IMGUI_ROOT}"
    "${ZA_IMGUI_ROOT}/backends"
    "${ZA_UPSTREAM_ROOT}/third_party/ooz"
    "${ZA_UPSTREAM_ROOT}/third_party/ryujinx"
)
target_compile_features(za_runtime_ui_compilecheck PRIVATE cxx_std_17)
target_compile_options(za_runtime_ui_compilecheck PRIVATE
    -Wall -Wextra -Wno-unused-function -Wno-format -fno-strict-aliasing
)
target_link_libraries(za_runtime_ui_compilecheck PRIVATE SDL3::SDL3 vulkan log android dl)

# Android Gradle externalNativeBuild explicitly builds shared-library targets.
# Make the upstream static compile gates hard dependencies of the app library,
# otherwise AGP can omit unreferenced static targets from its Ninja target list.
add_dependencies(main
    za_runtime_core_compilecheck
    za_runtime_graphics_compilecheck
    za_runtime_ui_compilecheck
)
message(STATUS "ZA_ANDROID_COMPILE_GATES_REQUIRED core=ON graphics=ON ui=ON")

