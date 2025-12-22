cmake_minimum_required(VERSION 3.10)
set(PROJECT gerber_explorer)

find_package(OpenGL REQUIRED)

add_executable(${PROJECT}
        main.cpp
        log_drawer.h
        log_drawer.cpp
        gl_base.cpp
        gl_functions.cpp
        gl_matrix.cpp
        gl_colors.h
        gl_drawer.h
        gl_function_list.h
        gl_functions.h
        gl_matrix.h
        glcorearb.h
        glext.h
        win32/gl_window.cpp
        win32/gl_window.h)

target_include_directories(${PROJECT} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/win32)

target_link_libraries(${PROJECT} PRIVATE gerber_lib libtess2 OpenGL::GL)
