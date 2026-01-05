cmake_minimum_required(VERSION 3.10)
include(FetchContent)

######################################################################
# JSON

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)

FetchContent_MakeAvailable(json)

include(libtess.cmake)
include(gl.cmake)
add_subdirectory(nativefiledialog-extended-1.2.1)

add_library(third_party INTERFACE)

target_link_libraries(third_party INTERFACE libtess2 nlohmann_json::nlohmann_json nfd gerber_gl)
