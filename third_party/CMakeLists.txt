cmake_minimum_required(VERSION 3.10)
include(../cmake/utils.cmake)
include(FetchContent)

######################################################################
# OPENGL

find_package(OpenGL REQUIRED)

######################################################################
# GLAD

set(GLAD_DIR ${CMAKE_CURRENT_LIST_DIR}/glad)

add_library(glad STATIC ${GLAD_DIR}/src/glad.c)

target_include_directories(glad PUBLIC ${GLAD_DIR}/include)

######################################################################
# GLFW

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

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
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_lib STATIC ${IMGUI_CORE_SOURCES} ${IMGUI_BACKEND_SOURCES})

target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends)

target_compile_definitions(imgui_lib PUBLIC IMGUI_USE_WCHAR32)

######################################################################
# main GL stuff

add_library(gerber_gl INTERFACE)

target_link_libraries(gerber_gl INTERFACE
        imgui_lib # gl and glfw are added via imgui_lib...
        glad)

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
)

FetchContent_GetProperties(libtess2)
if (NOT libtess2_POPULATED)
    FetchContent_Populate(libtess2)

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
endif ()

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
target_silence_warnings(Clipper2utils)
target_silence_warnings(Clipper2Zutils)

######################################################################
# Enable IPO/LTO for some libs

set(IPO_LIBS
        glfw
        glad
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

add_library(third_party INTERFACE
        debug-trap/debug-trap.h)

target_include_directories(third_party INTERFACE debug-trap)

target_link_libraries(third_party INTERFACE
        gerber_gl
        cpptrace::cpptrace
        DirectXMath
        nfd
        nlohmann_json::nlohmann_json
        libtess2
        Clipper2
        stb_image
)
