# Compile HLSL shaders to SPIR-V and DXIL at build time using DXC
# Usage: include this after FetchContent for DXC binary

include(FetchContent)

# Download prebuilt DXC release (build tool only, not shipped)
if(WIN32)
    set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip")
elseif(APPLE)
    set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz")
else()
    set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz")
endif()

FetchContent_Declare(dxc_binary
    URL ${DXC_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(dxc_binary)

if(WIN32)
    set(DXC_EXE "${dxc_binary_SOURCE_DIR}/bin/x64/dxc.exe" CACHE FILEPATH "DXC compiler" FORCE)
else()
    set(DXC_EXE "${dxc_binary_SOURCE_DIR}/bin/dxc" CACHE FILEPATH "DXC compiler" FORCE)
endif()

message(STATUS "DXC compiler: ${DXC_EXE}")

# Function to compile a single HLSL shader to SPIR-V (for Vulkan)
function(compile_hlsl_to_spirv SHADER_SOURCE SHADER_STAGE ENTRY_POINT OUTPUT_SPV)
    if(SHADER_STAGE STREQUAL "vertex")
        set(DXC_TARGET "vs_6_0")
    elseif(SHADER_STAGE STREQUAL "fragment")
        set(DXC_TARGET "ps_6_0")
    else()
        message(FATAL_ERROR "Unknown shader stage: ${SHADER_STAGE}")
    endif()

    add_custom_command(
        OUTPUT ${OUTPUT_SPV}
        COMMAND ${DXC_EXE} -T ${DXC_TARGET} -E ${ENTRY_POINT} -spirv
                -fspv-target-env=vulkan1.1
                -Fo ${OUTPUT_SPV}
                ${SHADER_SOURCE}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "HLSL -> SPIR-V: ${SHADER_SOURCE}"
        VERBATIM
    )
endfunction()

# Function to compile a single HLSL shader to DXIL (for D3D12)
function(compile_hlsl_to_dxil SHADER_SOURCE SHADER_STAGE ENTRY_POINT OUTPUT_DXIL)
    if(SHADER_STAGE STREQUAL "vertex")
        set(DXC_TARGET "vs_6_0")
    elseif(SHADER_STAGE STREQUAL "fragment")
        set(DXC_TARGET "ps_6_0")
    else()
        message(FATAL_ERROR "Unknown shader stage: ${SHADER_STAGE}")
    endif()

    add_custom_command(
        OUTPUT ${OUTPUT_DXIL}
        COMMAND ${DXC_EXE} -T ${DXC_TARGET} -E ${ENTRY_POINT}
                -Fo ${OUTPUT_DXIL}
                ${SHADER_SOURCE}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "HLSL -> DXIL: ${SHADER_SOURCE}"
        VERBATIM
    )
endfunction()
