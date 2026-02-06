include(CheckCXXCompilerFlag)

add_library(project_options INTERFACE)

set(IS_CLANG_CL FALSE)
if (MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(IS_CLANG_CL TRUE)
endif ()

set(SUPPRESSED_WARNINGS
        -Wno-unused-function
        -Wno-empty-body
        -Wno-missing-field-initializers
        -Wno-unused-parameter
        -Wno-implicit-fallthrough
        -Wno-nan-infinity-disabled
        -Wno-microsoft-anon-tag
        -Wno-defaulted-function-deleted
        -Wno-deprecated-declarations
        -Wno-missing-braces
        -Wno-unused-const-variable
        -Wno-multichar
)

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
    else ()
        list(APPEND DESIRED_FLAGS ${SUPPRESSED_WARNINGS})
    endif ()

    set(RELEASE_ONLY_FLAGS
            /Ot
            /fp:fast
            /Oi
    )

    # Clang-CL needs explicit -O3 for aggressive optimization (MSVC gets /O2 by default from CMake)
    if (IS_CLANG_CL)
        list(APPEND RELEASE_ONLY_FLAGS
                /clang:-O3
                /clang:-ffast-math
        )
    endif ()

else ()
    set(DESIRED_FLAGS
            -Wall
            -Wextra
            -Werror
            ${SUPPRESSED_WARNINGS}
    )

    set(RELEASE_ONLY_FLAGS
            -O3
            -ffast-math
            -flto            # Full LTO (not thin) to match MSVC /GL behavior
    )

    # Add LTO to link flags as well (required for Clang LTO)
    add_link_options($<$<CONFIG:Release,RelWithDebInfo>:-flto>)
endif ()

set(FINAL_FLAGS)

foreach (flag ${DESIRED_FLAGS})
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_NAME "HAS_${flag}")
    check_cxx_compiler_flag("${flag}" ${SAFE_NAME})
    if (${SAFE_NAME})
        list(APPEND FINAL_FLAGS ${flag})
        target_compile_options(project_options INTERFACE "${flag}")
    endif ()
endforeach ()

target_compile_definitions(project_options INTERFACE
    $<$<CONFIG:Debug>:_DEBUG>
)

# special x64 options
target_compile_options(project_options INTERFACE
    $<$<STREQUAL:"${CMAKE_SYSTEM_PROCESSOR}","x86_64">:-mavx2>
    $<$<STREQUAL:"${CMAKE_SYSTEM_PROCESSOR}","x86_64">:-mfma>
)

foreach (flag ${RELEASE_ONLY_FLAGS})
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" SAFE_NAME "HAS_${flag}")
    check_cxx_compiler_flag("${flag}" ${SAFE_NAME})
    if (${SAFE_NAME})
        list(APPEND FINAL_FLAGS ${flag})
        target_compile_options(project_options INTERFACE
                $<$<CONFIG:Release,RelWithDebInfo>:${flag}>
        )
    endif ()
endforeach ()

set(DEFINITIONS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
if (WIN32)
    list(APPEND DEFINITIONS WIN32_LEAN_AND_MEAN NOMINMAX)
endif ()

message("COMPILER OPTIONS: ${FINAL_FLAGS}")
message("COMPILER DEFINITIONS: ${DEFINITIONS}")

target_compile_definitions(project_options INTERFACE ${DEFINITIONS})
