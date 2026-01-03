cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0069 NEW)
project(gerber_explorer VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LTO

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT error)
set(IPO_SUPPORTED ${result} CACHE INTERNAL "Is LTO/IPO supported")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)

add_subdirectory(third_party)
add_subdirectory(gerber_lib)
add_subdirectory(gerber_explorer)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gerber_explorer)
set_property(TARGET gerber_explorer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
