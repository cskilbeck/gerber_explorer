cmake_minimum_required(VERSION 3.24)

include(${cmrc_SOURCE_DIR}/CMakeRC.cmake)

######################################################################
### Fonts for ImGui

cmrc_add_resource_library(my_assets
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "assets/icon.png"
        "assets/Roboto-Medium.ttf"
        "assets/MaterialIcons-Regular.ttf")

######################################################################
### Compile HLSL shaders at build time

include(${CMAKE_SOURCE_DIR}/cmake/compile_shaders.cmake)

set(HLSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders_hlsl")

# Shader names (stem only)
set(VERT_SHADERS solid color layer line line2 arc blit)
set(FRAG_SHADERS common layer line line2 arc blit selection)

# Only the shaders that gpu_base.cpp actually loads need an MSL counterpart.
# (line/arc HLSL shaders are not currently used at runtime.)
set(VERT_SHADERS_USED solid color layer line2 blit)
set(FRAG_SHADERS_USED common layer line2 blit selection)

if(APPLE)
    # ---- MSL source (Metal, macOS only) ----
    # We embed the .metal text directly and let SDL_GPU compile it with
    # MTLLibrary's source compiler at runtime. This makes shader errors
    # actionable (the alternative, .metallib, surfaces XPC failures from
    # the GPU pipeline compiler with no useful diagnostics).
    set(MSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders_msl")

    set(ALL_MSL_FILES "")
    foreach(S ${VERT_SHADERS_USED})
        list(APPEND ALL_MSL_FILES ${MSL_DIR}/${S}.vert.metal)
    endforeach()
    foreach(S ${FRAG_SHADERS_USED})
        list(APPEND ALL_MSL_FILES ${MSL_DIR}/${S}.frag.metal)
    endforeach()

    cmrc_add_resource_library(my_shaders_msl WHENCE "${MSL_DIR}" ${ALL_MSL_FILES})
else()
    # ---- SPIR-V (Vulkan) ----
    set(SHADER_SPV_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_spv")
    file(MAKE_DIRECTORY ${SHADER_SPV_DIR})

    set(ALL_SPV_FILES "")
    foreach(S ${VERT_SHADERS})
        compile_hlsl_to_spirv(${HLSL_DIR}/${S}.vert.hlsl vertex main ${SHADER_SPV_DIR}/${S}.vert.spv)
        list(APPEND ALL_SPV_FILES ${SHADER_SPV_DIR}/${S}.vert.spv)
    endforeach()
    foreach(S ${FRAG_SHADERS})
        compile_hlsl_to_spirv(${HLSL_DIR}/${S}.frag.hlsl fragment main ${SHADER_SPV_DIR}/${S}.frag.spv)
        list(APPEND ALL_SPV_FILES ${SHADER_SPV_DIR}/${S}.frag.spv)
    endforeach()

    cmrc_add_resource_library(my_shaders_spv WHENCE "${SHADER_SPV_DIR}" ${ALL_SPV_FILES})
endif()

# ---- DXIL (D3D12, Windows only) ----
if(WIN32)
    set(SHADER_DXIL_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_dxil")
    file(MAKE_DIRECTORY ${SHADER_DXIL_DIR})

    set(ALL_DXIL_FILES "")
    foreach(S ${VERT_SHADERS})
        compile_hlsl_to_dxil(${HLSL_DIR}/${S}.vert.hlsl vertex main ${SHADER_DXIL_DIR}/${S}.vert.dxil)
        list(APPEND ALL_DXIL_FILES ${SHADER_DXIL_DIR}/${S}.vert.dxil)
    endforeach()
    foreach(S ${FRAG_SHADERS})
        compile_hlsl_to_dxil(${HLSL_DIR}/${S}.frag.hlsl fragment main ${SHADER_DXIL_DIR}/${S}.frag.dxil)
        list(APPEND ALL_DXIL_FILES ${SHADER_DXIL_DIR}/${S}.frag.dxil)
    endforeach()

    cmrc_add_resource_library(my_shaders_dxil WHENCE "${SHADER_DXIL_DIR}" ${ALL_DXIL_FILES})
endif()

######################################################################
# MAIN

set(PROJECT gerber_explorer)

set(PROJECT_SOURCES
        main.cpp
        gpu_colors.h
        log_drawer.h
        log_drawer.cpp
        gpu_matrix.h
        gpu_matrix.cpp
        gerber_drawer.h
        gerber_drawer.cpp
        gpu_3d_drawer.h
        gpu_3d_drawer.cpp
        gpu_window.h
        gpu_window.cpp
        gpu_base.h
        gpu_base.cpp
        gpu_drawer.h
        gpu_drawer.cpp
        gpu_render.cpp
        gerber_explorer.cpp
        gerber_explorer.h
        util.h
        util.cpp
        settings.cpp
        settings.h
        job_pool.h
        job_pool.cpp
)

if (WIN32)
    list(APPEND PROJECT_SOURCES resources.rc)
endif ()

add_executable(${PROJECT} ${PROJECT_SOURCES})

# When using non-Apple Clang (e.g. Homebrew LLVM), ensure the linker can find
# its libc++ and libunwind. This applies to all build types.
if (APPLE AND NOT CMAKE_CXX_COMPILER MATCHES "^/usr/")
    get_filename_component(_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_LLVM_ROOT "${_COMPILER_DIR}" DIRECTORY)
    set(_LIBCXX_PATH "${_LLVM_ROOT}/lib/c++/libc++.1.dylib")
    if (EXISTS "${_LIBCXX_PATH}")
        message(STATUS "Using non-system libc++: ${_LIBCXX_PATH}")
        # clang already adds -lc++ implicitly; we only need to point the linker
        # at Homebrew's lib dirs and add -lunwind. Adding -lc++ ourselves causes
        # ld's "duplicate libraries" warning.
        target_link_options(${PROJECT} PRIVATE
            -L${_LLVM_ROOT}/lib/c++
            -L${_LLVM_ROOT}/lib/unwind
            -lunwind
            -Wl,-no_warn_duplicate_libraries
        )
        set(_USING_LLVM_LIBCXX TRUE)
    endif()
endif()

# Debug: Terminal application (for debug output)
# Release/RelWithDebInfo: App (with icon, no console etc)
if (APPLE AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_MACOSX_BUNDLE TRUE)
    set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/application.icns")

    set_source_files_properties(${ICON_PATH} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_target_properties(${PROJECT} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE "application.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.chs.gerber-explorer"
        MACOSX_BUNDLE_BUNDLE_NAME "Gerber Explorer"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION_MAJOR}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 Charlie Skilbeck. All rights reserved."
    )
    target_sources(${PROJECT} PRIVATE ${ICON_PATH})

    # Bundle non-system libc++ into the .app when building with Homebrew LLVM.
    if (_USING_LLVM_LIBCXX)
        get_filename_component(_LIBCXX_DIR "${_LIBCXX_PATH}" DIRECTORY)
        set(_LIBCXXABI_PATH "${_LIBCXX_DIR}/libc++abi.1.dylib")
        set(_LIBUNWIND_PATH "${_LLVM_ROOT}/lib/unwind/libunwind.1.dylib")

        message(STATUS "Bundling non-system libc++: ${_LIBCXX_PATH}")

        set_target_properties(${PROJECT} PROPERTIES
            INSTALL_RPATH "@executable_path/../Frameworks"
            BUILD_WITH_INSTALL_RPATH TRUE
        )

        # Helper list of dylibs to bundle: original_path, bundle_name
        # Start with libc++ (always present at this point). We strip each
        # dylib's existing codesignature before running install_name_tool;
        # otherwise install_name_tool warns that its edits invalidate the
        # signature. We re-codesign the whole bundle below.
        add_custom_command(TARGET ${PROJECT} POST_BUILD
            COMMENT "Bundling LLVM runtime libraries into app bundle..."
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_LIBCXX_PATH}"
                "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libc++.1.dylib"
            COMMAND codesign --remove-signature
                "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libc++.1.dylib"
            COMMAND install_name_tool -id
                "@rpath/libc++.1.dylib"
                "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libc++.1.dylib"
            COMMAND install_name_tool -change
                "${_LIBCXX_PATH}"
                "@rpath/libc++.1.dylib"
                "$<TARGET_FILE:${PROJECT}>"
        )

        if (EXISTS "${_LIBCXXABI_PATH}")
            message(STATUS "Bundling non-system libc++abi: ${_LIBCXXABI_PATH}")
            add_custom_command(TARGET ${PROJECT} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_LIBCXXABI_PATH}"
                    "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libc++abi.1.dylib"
                COMMAND codesign --remove-signature
                    "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libc++abi.1.dylib"
                COMMAND install_name_tool -id
                    "@rpath/libc++abi.1.dylib"
                    "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libc++abi.1.dylib"
                COMMAND install_name_tool -change
                    "${_LIBCXXABI_PATH}"
                    "@rpath/libc++abi.1.dylib"
                    "$<TARGET_FILE:${PROJECT}>"
                COMMAND install_name_tool -change
                    "${_LIBCXXABI_PATH}"
                    "@rpath/libc++abi.1.dylib"
                    "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libc++.1.dylib"
            )
        endif()

        if (EXISTS "${_LIBUNWIND_PATH}")
            message(STATUS "Bundling non-system libunwind: ${_LIBUNWIND_PATH}")
            add_custom_command(TARGET ${PROJECT} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_LIBUNWIND_PATH}"
                    "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libunwind.1.dylib"
                COMMAND codesign --remove-signature
                    "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libunwind.1.dylib"
                COMMAND install_name_tool -id
                    "@rpath/libunwind.1.dylib"
                    "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks/libunwind.1.dylib"
                COMMAND install_name_tool -change
                    "${_LIBUNWIND_PATH}"
                    "@rpath/libunwind.1.dylib"
                    "$<TARGET_FILE:${PROJECT}>"
            )
        endif()

        # Ad-hoc codesign all bundled dylibs, then the app bundle itself.
        # Dylibs must be signed before the bundle or the bundle signature is invalid.
        add_custom_command(TARGET ${PROJECT} POST_BUILD
            COMMENT "Ad-hoc codesigning app bundle..."
            COMMAND ${CMAKE_COMMAND} -DBUNDLE_DIR=$<TARGET_BUNDLE_DIR:${PROJECT}>
                -P "${CMAKE_SOURCE_DIR}/cmake/codesign_bundle.cmake"
        )
    else()
        message(STATUS "Using system libc++ (no bundling needed)")
    endif()
endif()


if(USE_OUTPUT_DEBUG_STRING)
    message("Using OutputDebugString for logging")
    target_compile_definitions(${PROJECT} PRIVATE LOG_USE_OUTPUT_DEBUG_STRING=1)
endif()

target_include_directories(${PROJECT} PRIVATE ${CMRC_INCLUDE_DIR})

target_link_libraries(${PROJECT} PRIVATE gerber_lib third_party my_assets project_options)

if(APPLE)
    target_link_libraries(${PROJECT} PRIVATE my_shaders_msl)
    target_compile_definitions(${PROJECT} PRIVATE HAS_MSL_SHADERS=1)
else()
    target_link_libraries(${PROJECT} PRIVATE my_shaders_spv)
endif()

if(WIN32)
    target_link_libraries(${PROJECT} PRIVATE my_shaders_dxil)
    target_compile_definitions(${PROJECT} PRIVATE HAS_DXIL_SHADERS=1)
endif()

target_enable_ipo(${PROJECT})

if (WIN32)
    set_target_properties(${PROJECT} PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    )
    if (MSVC)
        target_link_options(${PROJECT} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
        )
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(${PROJECT} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Xlinker>
            $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
        )
    endif()
endif()

add_custom_command(TARGET ${PROJECT} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/print_size.cmake" "$<TARGET_FILE:${PROJECT}>"
        COMMENT "Calculating binary size..."
)
