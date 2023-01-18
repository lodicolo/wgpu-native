cmake_minimum_required(VERSION 3.11b)

project(triangle)

set(TARGET_NAME triangle)

add_executable(triangle main.c ../framework.c)

if(MSVC)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_WINDOWS)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
    set(OS_LIBRARIES "userenv" "ws2_32" "Dwmapi" "dbghelp" "d3dcompiler" "D3D12" "D3D11" "DXGI" "setupapi" "Bcrypt")
elseif(APPLE)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_MACOS)
    set(OS_LIBRARIES "-framework Cocoa" "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore")
    target_compile_options(${TARGET_NAME} PRIVATE -x objective-c)
else(MSVC)
if(USE_WAYLAND)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_LINUX_WAYLAND)
else(USE_WAYLAND)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_LINUX_X11)
endif(USE_WAYLAND)
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic)
endif(MSVC)

find_package(glfw3 3.3 REQUIRED
    HINTS "$ENV{GLFW3_INSTALL_DIR}"
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" __BUILD_TYPE_DIRECTORY_NAME)

set(__TARGET_DIRECTORY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../target")

if(TARGET_TRIPLE)
    set(__TARGET_DIRECTORY_PATH "${__TARGET_DIRECTORY_PATH}/${TARGET_TRIPLE}")
endif(TARGET_TRIPLE)

set(__TARGET_DIRECTORY_PATH "${__TARGET_DIRECTORY_PATH}//${__BUILD_TYPE_DIRECTORY_NAME}")

find_library(WGPU_LIBRARY wgpu_native
    HINTS "${__TARGET_DIRECTORY_PATH}"
)

target_include_directories(${TARGET_NAME} PUBLIC $ENV{GLFW3_INCLUDE_DIR})
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../ffi)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)

target_link_libraries(${TARGET_NAME} glfw ${WGPU_LIBRARY} ${OS_LIBRARIES})
