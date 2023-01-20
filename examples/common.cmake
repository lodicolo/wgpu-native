cmake_minimum_required(VERSION 3.11b)

macro(wgpu_native_setup_example)
    cmake_parse_arguments(
        EXAMPLE
        ""
        "TARGET"
        "SOURCES;EXTERNAL_DEPENDENCIES;BUILT_DEPENDENCIES"
        ${ARGN}
    )

    if (${EXAMPLE_TARGET} STREQUAL "")
        set(EXAMPLE_TARGET "${EXAMPLE_PROJECT}")
    endif()

    if (${EXAMPLE_SOURCES} STREQUAL "")
        message(FATAL_ERROR "wgpu_native_setup_example() needs to be called with at least one entry in SOURCES")
    endif()

    add_executable(${EXAMPLE_TARGET} ${EXAMPLE_SOURCES} ../framework.c)

    if(MSVC) # windows vc
        target_compile_definitions(${EXAMPLE_TARGET} PUBLIC WGPU_TARGET=WGPU_TARGET_WINDOWS)
        target_compile_options(${EXAMPLE_TARGET} PRIVATE /W4)
        set(OS_LIBRARIES "userenv" "ws2_32" "Dwmapi" "dbghelp" "d3dcompiler" "D3D12" "D3D11" "DXGI" "setupapi" "Bcrypt")
    elseif(WIN32) # windows mingw (including cross-compiler)
        # list(APPEND __COMPILE_DEFINITIONS WGPU_TARGET=WGPU_TARGET_WINDOWS)
        target_compile_definitions(${EXAMPLE_TARGET} PUBLIC WGPU_TARGET=WGPU_TARGET_WINDOWS)
        target_compile_options(${EXAMPLE_TARGET} PRIVATE -Wall -Wextra -pedantic -g -fstack-protector)
    elseif(APPLE) # MacOS or MacOS-like
        target_compile_definitions(${EXAMPLE_TARGET} PUBLIC WGPU_TARGET=WGPU_TARGET_MACOS)
        set(OS_LIBRARIES "-framework Cocoa" "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore")
        target_compile_options(${EXAMPLE_TARGET} PRIVATE -x objective-c)
    else() # Assume linux
        if(USE_WAYLAND)
            target_compile_definitions(${EXAMPLE_TARGET} PUBLIC WGPU_TARGET=WGPU_TARGET_LINUX_WAYLAND)
        else(USE_WAYLAND)
            target_compile_definitions(${EXAMPLE_TARGET} PUBLIC WGPU_TARGET=WGPU_TARGET_LINUX_X11)
        endif(USE_WAYLAND)
        target_compile_options(${EXAMPLE_TARGET} PRIVATE -Wall -Wextra -pedantic)
    endif()

    if ("glfw3" IN_LIST EXAMPLE_EXTERNAL_DEPENDENCIES)
        message(STATUS "Searching for wgpu_native in $ENV{GLFW3_INSTALL_DIR}")
        find_package(glfw3 3.3
            REQUIRED
            HINTS "$ENV{GLFW3_INSTALL_DIR}"
        )

        get_target_property(glfw_IMPORTED_IMPLIB glfw IMPORTED_IMPLIB)
        if (NOT glfw_IMPORTED_IMPLIB)
            set_property(TARGET glfw PROPERTY IMPORTED_IMPLIB "-lglfw3")
        endif()

        target_include_directories(${EXAMPLE_TARGET} PUBLIC $ENV{GLFW3_INCLUDE_DIR})
        target_link_libraries(${EXAMPLE_TARGET} glfw)

        if (WIN32 AND NOT MSVC)
            target_compile_options(${EXAMPLE_TARGET} PRIVATE -fstack-protector)
            target_link_libraries(${EXAMPLE_TARGET} ssp)
        endif()
    endif()

    string(TOLOWER "${CMAKE_BUILD_TYPE}" __BUILD_TYPE_DIRECTORY_NAME)

    cmake_path(SET __TARGET_DIRECTORY_PATH NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../../target")

    if(TARGET_TRIPLE)
        cmake_path(APPEND __TARGET_DIRECTORY_PATH "${TARGET_TRIPLE}")
    endif(TARGET_TRIPLE)

    list(APPEND CMAKE_FIND_ROOT_PATH "${__TARGET_DIRECTORY_PATH}")

    cmake_path(APPEND __TARGET_DIRECTORY_PATH "${__BUILD_TYPE_DIRECTORY_NAME}")

    message(STATUS "Searching for wgpu_native in ${__TARGET_DIRECTORY_PATH}")
    find_library(WGPU_LIBRARY
        NAMES wgpu_native
        HINTS "${__TARGET_DIRECTORY_PATH}"
        PATHS "/${__BUILD_TYPE_DIRECTORY_NAME}"
    )

    target_include_directories(${EXAMPLE_TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../ffi)
    target_include_directories(${EXAMPLE_TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
    target_link_libraries(${EXAMPLE_TARGET} ${WGPU_LIBRARY})

    if ("helper" IN_LIST EXAMPLE_BUILT_DEPENDENCIES)
        message(STATUS "Searching for helper in ${__TARGET_DIRECTORY_PATH}")
        find_library(HELPER_LIBRARY helper
            HINTS "${__TARGET_DIRECTORY_PATH}"
        )
        target_link_libraries(${EXAMPLE_TARGET} ${HELPER_LIBRARY})
    endif()

    # set_property(TARGET glfw PROPERTY IMPORTED_IMPLIB "-lglfw3")

    # set_property(SOURCE main.c PROPERTY COMPILE_DEFINITIONS ${__COMPILE_DEFINITIONS})

    # target_link_libraries(${TARGET_NAME} glfw ${WGPU_LIBRARY} ${OS_LIBRARIES})
endmacro(wgpu_native_setup_example)
