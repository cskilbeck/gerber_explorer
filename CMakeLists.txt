cmake_minimum_required(VERSION 3.24)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

project(gerber_explorer VERSION 1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(utils)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (${MSVC_IDE})
    message(STATUS "Environment: Visual Studio detected: [${MSVC_IDE}],[$ENV{VisualStudioVersion}],[$ENV{VSAPPIDNAME}]")
    set(USE_OUTPUT_DEBUG_STRING ON CACHE BOOL "" FORCE)
else()
    message(STATUS "Environment: Standard Terminal or CLion detected.")
    set(USE_OUTPUT_DEBUG_STRING OFF CACHE BOOL "" FORCE)
endif()

# Static/Dynamic runtime linking

option(STATIC_RUNTIME "Link statically with runtime libraries" OFF)

if(MSVC)
    # CMP0091=NEW (via cmake_minimum_required 3.24): CMake uses CMAKE_MSVC_RUNTIME_LIBRARY
    # instead of injecting /MD into CMAKE_*_FLAGS. This controls the runtime for all targets.
    if(STATIC_RUNTIME)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        message(STATUS "Runtime: static (/MT)")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        message(STATUS "Runtime: dynamic (/MD)")
    endif()
elseif(STATIC_RUNTIME)
    if(APPLE)
        message(WARNING "Static runtime linking is not supported on macOS. STATIC_RUNTIME will be ignored.")
    else()
        add_link_options(-static-libgcc -static-libstdc++)
        message(STATUS "Runtime: static (-static-libgcc -static-libstdc++)")
    endif()
else()
    message(STATUS "Runtime: dynamic (default)")
endif()

if(APPLE)
    # This ensures the C++23 library is embedded in the binary
    # without breaking the initial CMake C-compiler tests.
    add_link_options("-stdlib=libc++" "-static-libc++")
endif()

# ASAN

option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

if(ENABLE_ASAN)
    if(MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang-CL: compile flag instruments the code
        add_compile_options(/fsanitize=address /clang:-fno-omit-frame-pointer)
        # CMake invokes lld-link directly (not via clang-cl), so we must
        # find and link the ASAN runtime libraries explicitly.
        # CMake invokes lld-link directly (not via clang-cl), so we must
        # find and link the ASAN runtime libraries explicitly.
        # On Windows, ASAN is always a DLL; we link the import lib + thunk.
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} /clang:--print-file-name=lib/windows
            OUTPUT_VARIABLE CLANG_RT_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(ASAN_ARCH "x86_64")
        else()
            set(ASAN_ARCH "i386")
        endif()
        link_libraries(
            "${CLANG_RT_DIR}/clang_rt.asan_dynamic-${ASAN_ARCH}.lib")
        if(STATIC_RUNTIME)
            link_libraries(
                "${CLANG_RT_DIR}/clang_rt.asan_static_runtime_thunk-${ASAN_ARCH}.lib")
        else()
            link_libraries(
                "${CLANG_RT_DIR}/clang_rt.asan_dynamic_runtime_thunk-${ASAN_ARCH}.lib")
        endif()
        # Clang-CL ASAN doesn't support debug runtime libraries (/MTd, /MDd)
        if(STATIC_RUNTIME)
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
        else()
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        endif()
        # Non-debug CRT in Debug config causes _ITERATOR_DEBUG_LEVEL mismatches
        # between targets that define _DEBUG and those that don't. Force it to 0.
        add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
        # /RTC1 is incompatible with ASAN and forces debug CRT metadata
        foreach(flag_var CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
            string(REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
            set(${flag_var} "${${flag_var}}" CACHE STRING "" FORCE)
        endforeach()
    elseif(MSVC)
        # Real MSVC: both compile and link flags
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang (non-Windows)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

# TSAN

option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)

if(ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Thread Sanitizer enabled")
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
        add_compile_options(-g -fno-omit-frame-pointer)
    else()
        message(WARNING "TSan requested but compiler is ${CMAKE_CXX_COMPILER_ID}. TSan is only supported on Clang.")
    endif()
endif()

# LTO

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT error)
set(IPO_SUPPORTED ${result} CACHE INTERNAL "Is LTO/IPO supported")
message("IPO/LTO is ${IPO_SUPPORTED}")

include(compile_options)

add_subdirectory(third_party)
add_subdirectory(gerber_lib)
add_subdirectory(gerber_explorer)

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gerber_explorer)
set_property(TARGET gerber_explorer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
