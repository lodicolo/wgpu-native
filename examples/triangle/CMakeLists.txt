cmake_minimum_required(VERSION 3.11b)

project(triangle)

include(../common.cmake)

wgpu_native_setup_example(TARGET ${CMAKE_PROJECT_NAME} SOURCES main.c EXTERNAL_DEPENDENCIES glfw3)
