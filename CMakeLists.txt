cmake_minimum_required(VERSION 3.10)

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
