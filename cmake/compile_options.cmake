include(CheckCXXCompilerFlag)

add_library(project_options INTERFACE)

set(IS_CLANG_CL FALSE)
if (MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(IS_CLANG_CL TRUE)
endif()

if (MSVC)

    set(DESIRED_FLAGS 
        /wd4100
        /wd4505
        /wd4324 
        /arch:AVX2
        /WX
    )
    # sigh...
    if (NOT IS_CLANG_CL)
        list(APPEND DESIRED_FLAGS
            /MP
        )
    else()
        list(APPEND DESIRED_FLAGS
            -Wno-unused-function
            -Wno-empty-body 
            -Wno-missing-field-initializers
            -Wno-unused-parameter
            -Wno-implicit-fallthrough
            -Wno-microsoft-anon-tag
            -Wno-defaulted-function-deleted
            -Wno-missing-braces
            -Wno-unused-const-variable
            -Wno-multichar
        )
    endif()
    
    set(RELEASE_ONLY_FLAGS
        /Ot
        /fp:fast
        /Oi
    )

else()
    set(DESIRED_FLAGS
        -mavx2
        -mfma
        -Wall
        -Wextra
        -Werror
        -Wno-unused-function
        -Wno-empty-body
        -Wno-missing-field-initializers
        -Wno-unused-parameter
        -Wno-implicit-fallthrough
        -Wno-microsoft-anon-tag
        -Wno-multichar
    )
    set(RELEASE_ONLY_FLAGS
        -O3
    )
endif()

set(FINAL_FLAGS)

foreach(flag ${DESIRED_FLAGS})
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_NAME "HAS_${flag}")
    check_cxx_compiler_flag("${flag}" ${SAFE_NAME})
    if(${SAFE_NAME})
        list(APPEND FINAL_FLAGS ${flag})
        target_compile_options(project_options INTERFACE "${flag}")
    endif()
endforeach()

foreach(flag ${RELEASE_ONLY_FLAGS})
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_NAME "HAS_${flag}")
    check_cxx_compiler_flag("${flag}" ${SAFE_NAME})
    if(${SAFE_NAME})
        list(APPEND FINAL_FLAGS ${flag})
        target_compile_options(project_options INTERFACE 
            $<$<CONFIG:Release,RelWithDebInfo>:${flag}>
        )
    endif()
endforeach()

set(DEFINITIONS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
if (WIN32)
    list(APPEND DEFINITIONS WIN32_LEAN_AND_MEAN NOMINMAX)
endif ()

message("COMPILER OPTIONS: ${FINAL_FLAGS}")
message("COMPILER DEFINITIONS: ${DEFINITIONS}")

target_compile_definitions(project_options INTERFACE ${DEFINITIONS})
