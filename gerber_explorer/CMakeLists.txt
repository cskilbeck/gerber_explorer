cmake_minimum_required(VERSION 3.10)
include(FetchContent)

######################################################################
# CMRC (CMake Resource Compiler)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        cmrc
        GIT_REPOSITORY https://github.com/vector-of-bool/cmrc.git
        GIT_TAG 2.0.1)

FetchContent_MakeAvailable(cmrc)

list(APPEND CMAKE_MODULE_PATH "${cmrc_SOURCE_DIR}")

include(CMakeRC)

### Fonts for ImGui

cmrc_add_resource_library(my_assets
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/Roboto-Medium.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/MaterialIcons-Regular.ttf")

### GL Shader sources

cmrc_add_resource_library(my_shaders
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/color_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/layer_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/layer_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/line_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/line_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/line2_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/line2_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/arc_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/arc_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/solid_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_vertex_shader.glsl")

target_include_directories(my_assets PUBLIC assets)

######################################################################
# MAIN

set(PROJECT gerber_explorer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT}
        main.cpp
        gl_colors.h
        log_drawer.h
        log_drawer.cpp
        gl_base.h
        gl_base.cpp
        gl_matrix.h
        gl_matrix.cpp
        gl_drawer.h
        gl_drawer.cpp
        gl_window.h
        gl_window.cpp
        gerber_explorer.cpp
        gerber_explorer.h
        util.h
        util.cpp
        settings.cpp
        settings.h)

if (MSVC)
    string(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(${PROJECT} PRIVATE
            /MP       # multiprocessor build
            /wd4100   # unreferenced formal parameter
            /wd4324   # padding due to alignment
            /wd4505   # unreferenced function with internal linkage has been removed
            /arch:AVX2
            /WX)      # warnings-as-errors

else ()
    target_compile_options(${PROJECT} PRIVATE
            -mavx2
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wno-empty-body
            -Wno-unused-parameter
            -Wno-implicit-fallthrough
            -Wno-multichar)
endif ()

set_target_properties(${PROJECT} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${IPO_SUPPORTED})

target_compile_definitions(${PROJECT} PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

target_include_directories(${PROJECT} PRIVATE
        ${CMRC_INCLUDE_DIR})

target_link_libraries(${PROJECT} PRIVATE gerber_lib third_party my_assets my_shaders)

if (MSVC)
    set_target_properties(gerber_explorer PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif ()
