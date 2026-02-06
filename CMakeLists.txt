cmake_minimum_required(VERSION 3.24)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

project(gerber_explorer VERSION 1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(utils)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

if (${MSVC_IDE})
    message(STATUS "Environment: Visual Studio detected: [${MSVC_IDE}],[$ENV{VisualStudioVersion}],[$ENV{VSAPPIDNAME}]")
    set(USE_OUTPUT_DEBUG_STRING ON CACHE BOOL "" FORCE)
else()
    message(STATUS "Environment: Standard Terminal or CLion detected.")
    set(USE_OUTPUT_DEBUG_STRING OFF CACHE BOOL "" FORCE)
endif()

# TSAN

option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)

if(ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Thread Sanitizer enabled")
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
        add_compile_options(-g -fno-omit-frame-pointer)
    else()
        message(WARNING "TSan requested but compiler is ${CMAKE_CXX_COMPILER_ID}. TSan is only supported on Clang.")
    endif()
endif()

# LTO

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT error)
set(IPO_SUPPORTED ${result} CACHE INTERNAL "Is LTO/IPO supported")
message("IPO/LTO is ${IPO_SUPPORTED}")

include(compile_options)

add_subdirectory(third_party)
add_subdirectory(gerber_lib)
add_subdirectory(gerber_explorer)

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gerber_explorer)
set_property(TARGET gerber_explorer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
