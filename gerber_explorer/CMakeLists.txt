cmake_minimum_required(VERSION 3.10)
include(FetchContent)

######################################################################
# CMRC

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        cmrc
        GIT_REPOSITORY https://github.com/vector-of-bool/cmrc.git
        GIT_TAG 2.0.1)

FetchContent_MakeAvailable(cmrc)

list(APPEND CMAKE_MODULE_PATH "${cmrc_SOURCE_DIR}")

include(CMakeRC)

cmrc_add_resource_library(my_assets
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/Roboto-Medium.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/MaterialIcons-Regular.ttf")

cmrc_add_resource_library(my_shaders
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/color_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/layer_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/layer_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/line_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/line_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/solid_vertex_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_fragment_shader.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_vertex_shader.glsl")

######################################################################
# GLFW

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

######################################################################
# JSON

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)

FetchContent_MakeAvailable(json)

######################################################################
# IMGUI

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking)

FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

######################################################################
# OPENGL

find_package(OpenGL REQUIRED)

######################################################################
# MAIN

set(PROJECT gerber_explorer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT}
        ${IMGUI_SOURCES}
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
        settings.h
        assets/matsym_codepoints_utf8.h)

if (MSVC)
    string(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(${PROJECT} PRIVATE
            /MP       # multiprocessor build
            /wd4100   # unreferenced formal parameter
            /wd4505   # unreferenced function with internal linkage has been removed
            /WX)      # warnings-as-errors

else ()
    target_compile_options(${PROJECT} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wno-empty-body
            -Wno-unused-parameter
            -Wno-implicit-fallthrough
            -Wno-multichar)
endif ()

target_compile_definitions(${PROJECT} PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

target_include_directories(${PROJECT} PRIVATE
        ${CMRC_INCLUDE_DIR}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends)

target_link_libraries(${PROJECT} PRIVATE gerber_lib libtess2 glad glfw OpenGL::GL nlohmann_json::nlohmann_json nfd my_assets my_shaders)

if (MSVC)
    set_target_properties(gerber_explorer PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif ()
