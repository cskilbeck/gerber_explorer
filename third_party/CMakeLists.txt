cmake_minimum_required(VERSION 3.24)
include(../cmake/utils.cmake)
include(FetchContent)

# Remove default MSVC warning levels for all third-party code
if(MSVC)
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

######################################################################
# SDL3

FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.14)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3)

######################################################################
# IMGUI

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.5-docking
)

FetchContent_MakeAvailable(imgui)

file(GLOB IMGUI_CORE_SOURCES "${imgui_SOURCE_DIR}/*.cpp")
set(IMGUI_BACKEND_SOURCES
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp
)

add_library(imgui_lib STATIC ${IMGUI_CORE_SOURCES} ${IMGUI_BACKEND_SOURCES})

target_link_libraries(imgui_lib PUBLIC SDL3::SDL3-static)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(imgui_lib PUBLIC X11::X11)
endif()

target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends)

target_compile_definitions(imgui_lib PUBLIC IMGUI_USE_WCHAR32)

######################################################################
# main rendering libs

add_library(gerber_gpu INTERFACE)

target_link_libraries(gerber_gpu INTERFACE
        imgui_lib)

######################################################################
# cpptrace

set(cpptrace_SYSTEM ON)

FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v1.0.4
)

set(CPPTRACE_DISABLE_CXX_20_MODULES ON CACHE BOOL "Disable modules for cpptrace" FORCE)

FetchContent_MakeAvailable(cpptrace)

######################################################################
# SDL_shadercross

FetchContent_Declare(
        SDL_shadercross
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
        GIT_TAG main)

set(SDLSHADERCROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_STATIC ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_CLI OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_DXC OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL_shadercross)

######################################################################
# DirectXMath

FetchContent_Declare(
        directxmath
        GIT_REPOSITORY https://github.com/microsoft/DirectXMath.git
        GIT_TAG apr2025
)

FetchContent_MakeAvailable(directxmath)

######################################################################
# NativeFileDialog

FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended
        GIT_TAG v1.3.0
)

FetchContent_MakeAvailable(nfd)

######################################################################
# JSON

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(json)

######################################################################
# CMRC (CMake Resource Compiler)

FetchContent_Declare(
        cmrc
        GIT_REPOSITORY https://github.com/cskilbeck/cmrc_fork.git
        GIT_TAG release_21012026
)

FetchContent_MakeAvailable(cmrc)

set(cmrc_SOURCE_DIR "${cmrc_SOURCE_DIR}" PARENT_SCOPE)

######################################################################
# libTESS2

FetchContent_Declare(
        libtess2
        GIT_REPOSITORY https://github.com/memononen/libtess2.git
        GIT_TAG v1.0.2
        SOURCE_SUBDIR .dummy
)

FetchContent_MakeAvailable(libtess2)

file(GLOB TESS2_SOURCES
        "${libtess2_SOURCE_DIR}/Source/*.c"
        "${libtess2_SOURCE_DIR}/Source/*.h"
)

add_library(libtess2 STATIC ${TESS2_SOURCES})

target_include_directories(libtess2 PUBLIC
        "${libtess2_SOURCE_DIR}/Include"
)

target_include_directories(libtess2 PRIVATE "${libtess2_SOURCE_DIR}/Contrib")

target_silence_warnings(libtess2)

######################################################################
# stb_image

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        f1c79c02822848a9bed4315b12c8c8f3761e1296  # Or a specific commit hash for stability
)

# Download and make it available
FetchContent_MakeAvailable(stb)

# Create an INTERFACE library for your project to link against
add_library(stb_image INTERFACE)

# Add the downloaded directory to the include path for this target
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

######################################################################
# Clipper2

FetchContent_Declare(
        Clipper2
        GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
        GIT_TAG        Clipper2_2.0.1
        SOURCE_SUBDIR  CPP
)

set(CLIPPER2_TESTS OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_CLIPPER2_UTILS OFF CACHE BOOL "" FORCE)
set(CLIPPER2_UTILS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Clipper2)

target_silence_warnings(Clipper2)
target_silence_warnings(Clipper2Z)
target_silence_warnings(Clipper2utils)
target_silence_warnings(Clipper2Zutils)

######################################################################
# Enable IPO/LTO for some libs

set(IPO_LIBS
        SDL3-static
        imgui_lib
        DirectXMath
        nfd
        libtess2
        Clipper2
)

foreach(LIB ${IPO_LIBS})
    target_enable_ipo(${LIB})
endforeach()

######################################################################
# main 3rd party library

add_library(third_party INTERFACE)

if(WIN32)
    set_target_properties(Clipper2 PROPERTIES INTERFACE_LINK_LIBRARIES "")
    set_target_properties(libtess2 PROPERTIES INTERFACE_LINK_LIBRARIES "")
else()
    target_include_directories(third_party INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    if(APPLE)
        target_compile_definitions(third_party INTERFACE _XM_ARM_NEON_INTRINSICS_)
    endif()
endif()

target_link_libraries(third_party INTERFACE
        gerber_gpu
        cpptrace::cpptrace
        DirectXMath
        nfd
        nlohmann_json::nlohmann_json
        libtess2
        Clipper2
        stb_image
)
