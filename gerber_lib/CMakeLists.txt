cmake_minimum_required(VERSION 3.10)

set(PROJECT gerber_lib)

set(CMAKE_CXX_STANDARD 23)
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
        gerber_util.cpp
        gerber_arena.cpp
        gerber_arena.h)

set_target_properties(${PROJECT} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${IPO_SUPPORTED})

target_include_directories(${PROJECT} PUBLIC .)

if (MSVC)
    string(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(${PROJECT} PRIVATE
            /MP       # multiprocessor build
            /wd4100   # unreferenced formal parameter
            /wd4505   # unreferenced function with internal linkage has been removed
            /arch:AVX2
            /WX)      # warnings-as-errors

else ()
    target_compile_options(${PROJECT} PRIVATE
            -mavx2
            -mfma
            -Wall
            -Wextra
            -Wno-pedantic
            -Wno-strict-aliasing
            -Wno-unused-function
            -Werror
            -Wno-empty-body
            -Wno-unused-but-set-variable
            -Wno-unused-parameter
            -Wno-implicit-fallthrough
            -Wno-microsoft-anon-tag
            -Wno-multichar)
endif ()

target_compile_definitions(${PROJECT} PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
