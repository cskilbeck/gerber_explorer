cmake_minimum_required(VERSION 3.29)

set(GLAD_DIR ${CMAKE_CURRENT_LIST_DIR}/glad)

include_directories(${GLAD_DIR}/include)

add_library(glad STATIC ${GLAD_DIR}/src/glad.c)

target_include_directories(glad PUBLIC ${GLAD_DIR}/include)
