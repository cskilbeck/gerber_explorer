cmake_minimum_required(VERSION 3.10)

include(${cmrc_SOURCE_DIR}/CMakeRC.cmake)

######################################################################
### Fonts for ImGui

cmrc_add_resource_library(my_assets
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "assets/Roboto-Medium.ttf"
        "assets/MaterialIcons-Regular.ttf")

######################################################################
### GL Shader sources

cmrc_add_resource_library(my_shaders
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "shaders/color_vertex_shader.glsl"
        "shaders/common_fragment_shader.glsl"
        "shaders/layer_fragment_shader.glsl"
        "shaders/layer_vertex_shader.glsl"
        "shaders/line_fragment_shader.glsl"
        "shaders/line_vertex_shader.glsl"
        "shaders/line2_fragment_shader.glsl"
        "shaders/line2_vertex_shader.glsl"
        "shaders/arc_fragment_shader.glsl"
        "shaders/arc_vertex_shader.glsl"
        "shaders/solid_vertex_shader.glsl"
        "shaders/textured_fragment_shader.glsl"
        "shaders/textured_vertex_shader.glsl")

######################################################################
# MAIN

set(PROJECT gerber_explorer)

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
        settings.h
        job_pool.h
        job_pool.cpp)

target_include_directories(${PROJECT} PRIVATE ${CMRC_INCLUDE_DIR})

target_link_libraries(${PROJECT} PRIVATE gerber_lib third_party my_assets my_shaders project_options)

target_enable_ipo(${PROJECT})

if (MSVC)
    set_target_properties(${PROJECT} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif ()

add_custom_command(TARGET ${PROJECT} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/print_size.cmake" "$<TARGET_FILE:${PROJECT}>"
        COMMENT "Calculating binary size..."
)
