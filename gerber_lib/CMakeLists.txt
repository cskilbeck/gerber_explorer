cmake_minimum_required(VERSION 3.10)

set(PROJECT gerber_lib)

add_library(${PROJECT}
        gerber_util.h
        gerber_2d.cpp
        gerber_2d.h
        gerber_aperture.cpp
        gerber_aperture.h
        gerber_arc.h
        gerber_draw.h
        gerber_entity.h
        gerber_enums.cpp
        gerber_enums.h
        gerber_error.cpp
        gerber_error.h
        gerber_error_codes.h
        gerber_format.h
        gerber_image.cpp
        gerber_image.h
        gerber_level.cpp
        gerber_level.h
        gerber_lib.cpp
        gerber_lib.h
        gerber_log.cpp
        gerber_log.h
        gerber_math.h
        gerber_net.cpp
        gerber_net.h
        gerber_reader.cpp
        gerber_reader.h
        gerber_state.h
        gerber_stats.cpp
        gerber_stats.h
        gerber_util.cpp
        gerber_arena.cpp
        gerber_arena.h
        dx_math.h)

target_include_directories(${PROJECT} PUBLIC .)

target_link_libraries(${PROJECT} PRIVATE project_options)

target_enable_ipo(${PROJECT})
