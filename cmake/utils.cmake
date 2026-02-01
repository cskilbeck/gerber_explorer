######################################################################
# Enable Link Time Optimization for a target

function(target_enable_ipo TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        return()
    endif()

    if(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
        return()
    endif()

    set(REAL_TARGET ${TARGET_NAME})
    get_target_property(ALIAS_CHECK ${REAL_TARGET} ALIAS_FOR)
    while(ALIAS_CHECK)
        set(REAL_TARGET ${ALIAS_CHECK})
        get_target_property(ALIAS_CHECK ${REAL_TARGET} ALIAS_FOR)
    endwhile()

    get_target_property(TGT_TYPE ${REAL_TARGET} TYPE)
    if(TGT_TYPE STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    get_target_property(IS_IMPORTED ${REAL_TARGET} IMPORTED)
    if(IS_IMPORTED)
        return()
    endif()

    if(IPO_SUPPORTED)
        set_target_properties(${REAL_TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "IPO enabled for ${REAL_TARGET}")
    endif()
endfunction()

######################################################################
# Silence warnings for a target (used for some 3rd party libs)

function(target_silence_warnings TARGET_NAME)
    if(TARGET ${TARGET_NAME})
        set_target_properties(${TARGET_NAME} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
                $<TARGET_PROPERTY:${TARGET_NAME},INTERFACE_INCLUDE_DIRECTORIES>)

        if(MSVC)
            # Remove existing warning flags and add /w
            # Note: CMAKE_C/CXX_FLAGS /W[0..4] are removed in third_party/CMakeLists.txt to avoid D9025
            get_target_property(TARGET_OPTIONS ${TARGET_NAME} COMPILE_OPTIONS)
            if(TARGET_OPTIONS)
                set(FILTERED_OPTIONS "")
                foreach(OPT ${TARGET_OPTIONS})
                    if(NOT OPT MATCHES "^/[Ww]")
                        list(APPEND FILTERED_OPTIONS "${OPT}")
                    endif()
                endforeach()
                list(APPEND FILTERED_OPTIONS "/w")
                set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_OPTIONS "${FILTERED_OPTIONS}")
            else()
                set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_OPTIONS "/w")
            endif()
        else()
            target_compile_options(${TARGET_NAME} PRIVATE -w)
        endif()
    endif()
endfunction()
