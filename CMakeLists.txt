cmake_minimum_required(VERSION 3.10)

project(gerber_explorer VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(gerber_lib)
add_subdirectory(third_party)
add_subdirectory(gerber_explorer)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gerber_explorer)
set_property(TARGET gerber_explorer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
