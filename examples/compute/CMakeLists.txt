cmake_minimum_required(VERSION 3.11b)

include(../common.cmake)

wgpu_native_setup_example(PROJECT compute TARGET compute SOURCES main.c EXTERNAL_DEPENDENCIES glfw3)
