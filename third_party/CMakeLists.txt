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
include(glad.cmake)
include(gl.cmake)
add_subdirectory(nativefiledialog-extended-1.2.1)
