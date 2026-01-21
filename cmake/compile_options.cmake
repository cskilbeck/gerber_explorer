add_library(project_options INTERFACE)

if (MSVC)
    target_compile_options(project_options INTERFACE
            /MP
            /wd4100
            /wd4505
            /wd4324
            /arch:AVX2
            /WX
            $<$<CONFIG:Release,RelWithDebInfo>:/Ot /fp:fast /Oi>
    )

    set(FIX_FLAGS
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO
    )

    foreach (flag_var ${FIX_FLAGS})
        if (${flag_var} MATCHES "/Ob[12]")
            string(REGEX REPLACE "/Ob[12]" "/Ob3" ${flag_var} "${${flag_var}}")
        endif ()
    endforeach ()

else ()
    target_compile_options(project_options INTERFACE
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
endif ()

target_compile_definitions(project_options INTERFACE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
