cmake_minimum_required(VERSION 3.10)

include(${cmrc_SOURCE_DIR}/CMakeRC.cmake)

######################################################################
### Fonts for ImGui

cmrc_add_resource_library(my_assets
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "assets/icon.png"
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
        "shaders/blit_fragment_shader.glsl"
        "shaders/blit_vertex_shader.glsl")

######################################################################
# MAIN

set(PROJECT gerber_explorer)

set(PROJECT_SOURCES
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
        job_pool.cpp
        force_inline.h
)

if (WIN32)
    list(APPEND PROJECT_SOURCES resources.rc)
endif ()

add_executable(${PROJECT} ${PROJECT_SOURCES})

if(USE_OUTPUT_DEBUG_STRING)
    message("Using OutputDebugString for logging")
    target_compile_definitions(${PROJECT} PRIVATE LOG_USE_OUTPUT_DEBUG_STRING=1)
endif()

target_include_directories(${PROJECT} PRIVATE ${CMRC_INCLUDE_DIR})

target_link_libraries(${PROJECT} PRIVATE gerber_lib third_party my_assets my_shaders project_options)

target_enable_ipo(${PROJECT})

if (WIN32)
    # Debug: Console application (for debug output)
    # Release/RelWithDebInfo: Windows application (no console)
    set_target_properties(${PROJECT} PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    )
    if (MSVC)
        target_link_options(${PROJECT} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
        )
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(${PROJECT} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Xlinker>
            $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
        )
    endif()
endif()

add_custom_command(TARGET ${PROJECT} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/print_size.cmake" "$<TARGET_FILE:${PROJECT}>"
        COMMENT "Calculating binary size..."
)
