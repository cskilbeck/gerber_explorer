cmake_minimum_required(VERSION 3.10)
include(FetchContent)

######################################################################
# OPENGL

find_package(OpenGL REQUIRED)

######################################################################
# GLAD

set(GLAD_DIR ${CMAKE_CURRENT_LIST_DIR}/glad)

include_directories(${GLAD_DIR}/include)

add_library(glad STATIC ${GLAD_DIR}/src/glad.c)

target_include_directories(glad PUBLIC ${GLAD_DIR}/include)

######################################################################
# GLFW

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        SYSTEM)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

######################################################################
# cpptrace

set(cpptrace_SYSTEM ON)

FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v1.0.4
        SYSTEM)

FetchContent_MakeAvailable(cpptrace)

######################################################################
# DirectXMath

FetchContent_Declare(
        directxmath
        GIT_REPOSITORY https://github.com/microsoft/DirectXMath.git
        GIT_TAG        apr2025
        SYSTEM)

FetchContent_MakeAvailable(directxmath)

######################################################################
# NativeFileDialog

FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended
        GIT_TAG v1.3.0
        SYSTEM)

FetchContent_MakeAvailable(nfd)

######################################################################
# JSON

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        SYSTEM)

FetchContent_MakeAvailable(json)

######################################################################
# IMGUI

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        SYSTEM)

FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

add_library(gerber_gl ${IMGUI_SOURCES})

target_compile_definitions(gerber_gl PUBLIC IMGUI_USE_WCHAR32)

target_include_directories(gerber_gl PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends)

target_link_libraries(gerber_gl PUBLIC glfw glad OpenGL::GL DirectXMath)

######################################################################
# libTESS2

set(LIB_TESS2_DIR ${CMAKE_CURRENT_LIST_DIR}/libtess2-1.0.2)

include_directories(${LIB_TESS2_DIR}/Include ${LIB_TESS2_DIR}/Contrib)

add_library(libtess2 STATIC
        ${LIB_TESS2_DIR}/Source/bucketalloc.c
        ${LIB_TESS2_DIR}/Source/bucketalloc.h
        ${LIB_TESS2_DIR}/Source/dict.c
        ${LIB_TESS2_DIR}/Source/dict.h
        ${LIB_TESS2_DIR}/Source/geom.c
        ${LIB_TESS2_DIR}/Source/geom.h
        ${LIB_TESS2_DIR}/Source/mesh.c
        ${LIB_TESS2_DIR}/Source/mesh.h
        ${LIB_TESS2_DIR}/Source/priorityq.c
        ${LIB_TESS2_DIR}/Source/priorityq.h
        ${LIB_TESS2_DIR}/Source/sweep.c
        ${LIB_TESS2_DIR}/Source/sweep.h
        ${LIB_TESS2_DIR}/Source/tess.c
        ${LIB_TESS2_DIR}/Source/tess.h)

if(MSVC)
    # /wd4267 suppresses the 'size_t' to 'unsigned int' warning
    target_compile_options(libtess2 PRIVATE /wd4267)
endif()

target_include_directories(libtess2 PUBLIC ${LIB_TESS2_DIR}/Include)

######################################################################

add_library(third_party INTERFACE)

target_link_libraries(third_party INTERFACE libtess2 nlohmann_json::nlohmann_json nfd gerber_gl cpptrace::cpptrace)
