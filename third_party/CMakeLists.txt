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
        GIT_TAG 3.4)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

######################################################################
# cpptrace

FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v1.0.4)

FetchContent_MakeAvailable(cpptrace)

######################################################################
# DirectXMath

FetchContent_Declare(
        directxmath
        GIT_REPOSITORY https://github.com/microsoft/DirectXMath.git
        GIT_TAG        apr2025)

FetchContent_MakeAvailable(directxmath)

######################################################################
# IMGUI

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking)

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
# JSON

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)

FetchContent_MakeAvailable(json)

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
# NativeFileDialog

add_subdirectory(nativefiledialog-extended-1.2.1)

######################################################################

add_library(third_party INTERFACE)

target_link_libraries(third_party INTERFACE libtess2 nlohmann_json::nlohmann_json nfd gerber_gl cpptrace::cpptrace)
