cmake_minimum_required(VERSION 3.10)
set(PROJECT gerber_explorer)

include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)

FetchContent_MakeAvailable(json)

find_package(OpenGL REQUIRED)

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

target_include_directories(${PROJECT} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/win32)

target_link_libraries(${PROJECT} PRIVATE gerber_lib libtess2 glad glfw OpenGL::GL nlohmann_json::nlohmann_json)
