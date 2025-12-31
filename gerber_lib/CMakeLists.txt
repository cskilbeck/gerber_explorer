cmake_minimum_required(VERSION 3.10)

set(PROJECT gerber_lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    gerber_util.cpp)

target_include_directories(${PROJECT} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/${LIB_TESS2}/Include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)

if(MSVC)
    string(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(${PROJECT} PRIVATE
            /MP       # multiprocessor build
            /wd4100   # unreferenced formal parameter
            /wd4505   # unreferenced function with internal linkage has been removed
            /WX)      # warnings-as-errors

else()
    target_compile_options(${PROJECT} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wno-empty-body
            -Wno-unused-parameter
            -Wno-implicit-fallthrough
            -Wno-multichar)
endif()

target_compile_definitions(${PROJECT} PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

target_compile_features(${PROJECT} PUBLIC cxx_std_20)
