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
### GL Shader sources

cmrc_add_resource_library(my_shaders
        WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "shaders/color_vertex_shader.glsl"
        "shaders/common_fragment_shader.glsl"
        "shaders/layer_fragment_shader.glsl"
        "shaders/layer_vertex_shader.glsl"
        "shaders/line_fragment_shader.glsl"
        "shaders/line_vertex_shader.glsl"
        "shaders/line2_fragment_shader.glsl"
        "shaders/line2_vertex_shader.glsl"
        "shaders/arc_fragment_shader.glsl"
        "shaders/arc_vertex_shader.glsl"
        "shaders/solid_vertex_shader.glsl"
        "shaders/blit_fragment_shader.glsl"
        "shaders/selection_fragment_shader.glsl"
        "shaders/blit_vertex_shader.glsl")

######################################################################
# MAIN

set(PROJECT gerber_explorer)

set(PROJECT_SOURCES
        main.cpp
        gl_colors.h
        log_drawer.h
        log_drawer.cpp
        gl_base.h
        gl_base.cpp
        gl_matrix.h
        gl_matrix.cpp
        gl_drawer.h
        gl_drawer.cpp
        gl_3d_drawer.h
        gl_3d_drawer.cpp
        gl_window.h
        gl_window.cpp
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
        target_link_options(${PROJECT} PRIVATE
            -L${_LLVM_ROOT}/lib/c++
            -L${_LLVM_ROOT}/lib/unwind
            -lc++
            -lunwind
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
        # Start with libc++ (always present at this point)
        add_custom_command(TARGET ${PROJECT} POST_BUILD
            COMMENT "Bundling LLVM runtime libraries into app bundle..."
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${PROJECT}>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_LIBCXX_PATH}"
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

target_link_libraries(${PROJECT} PRIVATE gerber_lib third_party my_assets my_shaders project_options)

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
