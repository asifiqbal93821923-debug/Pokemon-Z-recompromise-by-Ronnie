cmake_minimum_required(VERSION 3.22.1)
project(za_android_probe C)

add_library(za_android_probe SHARED native_probe.c)
target_compile_features(za_android_probe PRIVATE c_std_11)
target_compile_options(za_android_probe PRIVATE -Wall -Wextra -Werror)
