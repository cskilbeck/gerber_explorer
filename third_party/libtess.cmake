cmake_minimum_required(VERSION 3.29)

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
