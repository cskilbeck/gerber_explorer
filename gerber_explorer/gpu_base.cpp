//////////////////////////////////////////////////////////////////////
// SDL_GPU rendering backend implementation

#include <cstring>

#include <cmrc/cmrc.hpp>

#include "gerber_log.h"
#include "gpu_base.h"

LOG_CONTEXT("gpu_base", info);

CMRC_DECLARE(my_shaders_spv);

#if HAS_DXIL_SHADERS
CMRC_DECLARE(my_shaders_dxil);
#endif

namespace
{
    struct shader_bytecode
    {
        char const *data;
        size_t size;
    };

    shader_bytecode load_resource(cmrc::embedded_filesystem &fs, char const *name)
    {
        if(!fs.is_file(name)) {
            LOG_ERROR("Shader not found: {}", name);
            return { nullptr, 0 };
        }
        auto f = fs.open(name);
        return { f.begin(), f.size() };
    }
}    // namespace

namespace gpu
{
    //////////////////////////////////////////////////////////////////////
    // Device

    bool device::init(SDL_Window *win)
    {
        window = win;

        // Request all shader formats we have pre-compiled
        SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV;
#if HAS_DXIL_SHADERS
        formats |= SDL_GPU_SHADERFORMAT_DXIL;
#endif

        LOG_INFO("Creating SDL_GPU device");
        // Prefer D3D12 on Windows, falls back to Vulkan
        char const *preferred_driver = nullptr;
#ifdef _WIN32
        preferred_driver = "direct3d12";
#endif
        gpu = SDL_CreateGPUDevice(formats, true, preferred_driver);
        if(!gpu) {
            LOG_ERROR("SDL_CreateGPUDevice failed: {}", SDL_GetError());
            return false;
        }

        LOG_INFO("GPU driver: {}", SDL_GetGPUDeviceDriver(gpu));
        shader_formats = SDL_GetGPUShaderFormats(gpu);
        LOG_INFO("Shader formats: SPIRV={} DXIL={} DXBC={} MSL={}",
                 (shader_formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0,
                 (shader_formats & SDL_GPU_SHADERFORMAT_DXIL) != 0,
                 (shader_formats & SDL_GPU_SHADERFORMAT_DXBC) != 0,
                 (shader_formats & SDL_GPU_SHADERFORMAT_MSL) != 0);

        if(!SDL_ClaimWindowForGPUDevice(gpu, window)) {
            LOG_ERROR("SDL_ClaimWindowForGPUDevice failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    void device::shutdown()
    {
        if(gpu && window) {
            SDL_ReleaseWindowFromGPUDevice(gpu, window);
        }
        if(gpu) {
            SDL_DestroyGPUDevice(gpu);
            gpu = nullptr;
        }
    }

    SDL_GPUShader *device::load_shader(char const *shader_name, char const *entry_point, SDL_GPUShaderStage stage,
                                        int num_samplers, int num_storage_textures, int num_storage_buffers, int num_uniform_buffers)
    {
        SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
        shader_bytecode code{};

#if HAS_DXIL_SHADERS
        if(shader_formats & SDL_GPU_SHADERFORMAT_DXIL) {
            // Build DXIL resource name: "solid.vert.dxil" from "solid.vert"
            std::string dxil_name = std::string(shader_name) + ".dxil";
            auto fs = cmrc::my_shaders_dxil::get_filesystem();
            code = load_resource(fs, dxil_name.c_str());
            if(code.data) {
                format = SDL_GPU_SHADERFORMAT_DXIL;
            }
        }
#endif

        if(format == SDL_GPU_SHADERFORMAT_INVALID) {
            // Fall back to SPIR-V
            std::string spv_name = std::string(shader_name) + ".spv";
            auto fs = cmrc::my_shaders_spv::get_filesystem();
            code = load_resource(fs, spv_name.c_str());
            if(code.data) {
                format = SDL_GPU_SHADERFORMAT_SPIRV;
            }
        }

        if(!code.data) {
            LOG_ERROR("No shader bytecode found for '{}'", shader_name);
            return nullptr;
        }

        LOG_INFO("  Loading '{}' ({}, {} bytes)", shader_name,
                 (format == SDL_GPU_SHADERFORMAT_DXIL) ? "DXIL" : "SPIR-V", code.size);

        SDL_GPUShaderCreateInfo ci{};
        ci.code = reinterpret_cast<Uint8 const *>(code.data);
        ci.code_size = code.size;
        ci.entrypoint = entry_point;
        ci.format = format;
        ci.stage = stage;
        ci.num_samplers = num_samplers;
        ci.num_storage_textures = num_storage_textures;
        ci.num_storage_buffers = num_storage_buffers;
        ci.num_uniform_buffers = num_uniform_buffers;

        SDL_GPUShader *shader = SDL_CreateGPUShader(gpu, &ci);
        if(!shader) {
            LOG_ERROR("SDL_CreateGPUShader failed for '{}': {}", shader_name, SDL_GetError());
        }
        return shader;
    }

    SDL_GPUBuffer *device::create_buffer(SDL_GPUBufferUsageFlags usage, uint32_t size, char const *name)
    {
        SDL_GPUBufferCreateInfo ci{};
        ci.usage = usage;
        ci.size = size;
        if(name) {
            ci.props = SDL_CreateProperties();
            SDL_SetStringProperty(ci.props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, name);
        }
        SDL_GPUBuffer *buf = SDL_CreateGPUBuffer(gpu, &ci);
        if(ci.props) {
            SDL_DestroyProperties(ci.props);
        }
        return buf;
    }

    SDL_GPUTransferBuffer *device::create_transfer_buffer(SDL_GPUTransferBufferUsage usage, uint32_t size)
    {
        SDL_GPUTransferBufferCreateInfo ci{};
        ci.usage = usage;
        ci.size = size;
        return SDL_CreateGPUTransferBuffer(gpu, &ci);
    }

    void device::upload_to_buffer(SDL_GPUBuffer *buffer, void const *data, uint32_t size)
    {
        SDL_GPUTransferBuffer *transfer = create_transfer_buffer(SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size);
        void *mapped = SDL_MapGPUTransferBuffer(gpu, transfer, false);
        memcpy(mapped, data, size);
        SDL_UnmapGPUTransferBuffer(gpu, transfer);

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTransferBufferLocation src{};
        src.transfer_buffer = transfer;
        src.offset = 0;
        SDL_GPUBufferRegion dst{};
        dst.buffer = buffer;
        dst.offset = 0;
        dst.size = size;
        SDL_UploadToGPUBuffer(copy, &src, &dst, false);

        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(cmd);

        SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    }

    SDL_GPUTexture *device::create_texture(uint32_t width, uint32_t height, SDL_GPUTextureFormat format,
                                            SDL_GPUTextureUsageFlags usage, SDL_GPUSampleCount samples)
    {
        SDL_GPUTextureCreateInfo ci{};
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = format;
        ci.width = width;
        ci.height = height;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.sample_count = samples;
        ci.usage = usage;
        return SDL_CreateGPUTexture(gpu, &ci);
    }

    SDL_GPUGraphicsPipeline *device::create_pipeline(SDL_GPUGraphicsPipelineCreateInfo const &info)
    {
        return SDL_CreateGPUGraphicsPipeline(gpu, &info);
    }

    void device::release_buffer(SDL_GPUBuffer *buffer)
    {
        if(buffer) SDL_ReleaseGPUBuffer(gpu, buffer);
    }

    void device::release_texture(SDL_GPUTexture *texture)
    {
        if(texture) SDL_ReleaseGPUTexture(gpu, texture);
    }

    void device::release_pipeline(SDL_GPUGraphicsPipeline *pipeline)
    {
        if(pipeline) SDL_ReleaseGPUGraphicsPipeline(gpu, pipeline);
    }

    void device::release_shader(SDL_GPUShader *shader)
    {
        if(shader) SDL_ReleaseGPUShader(gpu, shader);
    }

    void device::release_sampler(SDL_GPUSampler *sampler)
    {
        if(sampler) SDL_ReleaseGPUSampler(gpu, sampler);
    }

    //////////////////////////////////////////////////////////////////////
    // Render target

    void render_target::init(device &dev, uint32_t w, uint32_t h, SDL_GPUSampleCount samples)
    {
        cleanup(dev);
        width = w;
        height = h;
        sample_count = samples;

        if(samples != SDL_GPU_SAMPLECOUNT_1) {
            // MSAA: color texture for rendering, resolve texture for sampling
            color_texture = dev.create_texture(w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                               SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, samples);
            resolve_texture = dev.create_texture(w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                                  SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, SDL_GPU_SAMPLECOUNT_1);
        } else {
            // No MSAA: single texture for both rendering and sampling
            color_texture = dev.create_texture(w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                               SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, SDL_GPU_SAMPLECOUNT_1);
            resolve_texture = nullptr;
        }

        LOG_INFO("Render target created: {}x{}, {} samples", w, h, (int)samples);
    }

    void render_target::cleanup(device &dev)
    {
        dev.release_texture(color_texture);
        dev.release_texture(resolve_texture);
        color_texture = nullptr;
        resolve_texture = nullptr;
        width = 0;
        height = 0;
    }

    //////////////////////////////////////////////////////////////////////
    // Pipeline creation

    bool pipelines::init(device &dev, SDL_GPUTextureFormat swapchain_format, SDL_GPUSampleCount msaa_samples)
    {
        LOG_INFO("Compiling HLSL shaders and creating pipelines");

        // Load pre-compiled SPIR-V shaders and create GPU shader objects
        // solid: VS(0 samplers, 0 storage tex, 0 storage buf, 1 uniform) + common FS(0,0,0,0)
        LOG_INFO("Loading solid shaders");
        auto solid_vs = dev.load_shader("solid.vert", "main", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        auto common_fs = dev.load_shader("common.frag", "main", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);
        if(!solid_vs || !common_fs) return false;

        // color: VS(0,0,0,1) + common FS
        LOG_INFO("Loading color shaders");
        auto color_vs = dev.load_shader("color.vert", "main", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        if(!color_vs) return false;

        // layer: VS(0,0,1 storage buf,1 uniform) + FS(0,0,0,1 uniform)
        LOG_INFO("Loading layer shaders");
        auto layer_vs = dev.load_shader("layer.vert", "main", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1, 1);
        auto layer_fs = dev.load_shader("layer.frag", "main", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 1);
        if(!layer_vs || !layer_fs) return false;

        // line2: VS(0,0,3 storage bufs,1 uniform) + FS(0,0,0,1 uniform)
        LOG_INFO("Loading line2 shaders");
        auto line2_vs = dev.load_shader("line2.vert", "main", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 3, 1);
        auto line2_fs = dev.load_shader("line2.frag", "main", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 1);
        if(!line2_vs || !line2_fs) return false;

        // blit: VS(0,0,0,0) + FS(1 sampler, 0, 0, 1 uniform)
        LOG_INFO("Loading blit shaders");
        auto blit_vs = dev.load_shader("blit.vert", "main", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
        auto blit_fs = dev.load_shader("blit.frag", "main", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 1);
        if(!blit_vs || !blit_fs) return false;

        // selection: same VS as blit, FS(1 sampler, 0, 0, 1 uniform)
        LOG_INFO("Loading selection shaders");
        auto selection_fs = dev.load_shader("selection.frag", "main", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 1);
        if(!selection_fs) return false;

        // Create a nearest-neighbor sampler for the MSAA resolve texture
        SDL_GPUSamplerCreateInfo sampler_ci{};
        sampler_ci.min_filter = SDL_GPU_FILTER_NEAREST;
        sampler_ci.mag_filter = SDL_GPU_FILTER_NEAREST;
        sampler_ci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler_ci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler_ci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        nearest_sampler = SDL_CreateGPUSampler(dev.gpu, &sampler_ci);

        // ---- Create pipelines ----

        SDL_GPUTextureFormat rt_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

        // Helper: common blend state for alpha compositing
        auto alpha_blend = [](bool enable) -> SDL_GPUColorTargetBlendState {
            SDL_GPUColorTargetBlendState bs{};
            bs.enable_blend = enable;
            bs.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            bs.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            bs.color_blend_op = SDL_GPU_BLENDOP_ADD;
            bs.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            bs.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            bs.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            bs.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
            return bs;
        };

        auto premultiplied_blend = []() -> SDL_GPUColorTargetBlendState {
            SDL_GPUColorTargetBlendState bs{};
            bs.enable_blend = true;
            bs.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            bs.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            bs.color_blend_op = SDL_GPU_BLENDOP_ADD;
            bs.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            bs.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            bs.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            bs.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
            return bs;
        };

        auto additive_blend = []() -> SDL_GPUColorTargetBlendState {
            SDL_GPUColorTargetBlendState bs{};
            bs.enable_blend = true;
            bs.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            bs.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            bs.color_blend_op = SDL_GPU_BLENDOP_ADD;
            bs.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            bs.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            bs.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            bs.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
            return bs;
        };

        // ---- Solid pipeline (render to RT, alpha blend) ----
        {
            SDL_GPUColorTargetBlendState bs = alpha_blend(true);
            SDL_GPUColorTargetDescription ct{};
            ct.format = rt_format;
            ct.blend_state = bs;

            SDL_GPUVertexAttribute attrs[] = {
                { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0 },    // position
            };
            SDL_GPUVertexBufferDescription vb{};
            vb.slot = 0;
            vb.pitch = sizeof(vertex_solid);
            vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

            SDL_GPUGraphicsPipelineCreateInfo pi{};
            pi.vertex_shader = solid_vs;
            pi.fragment_shader = common_fs;
            pi.vertex_input_state.vertex_buffer_descriptions = &vb;
            pi.vertex_input_state.num_vertex_buffers = 1;
            pi.vertex_input_state.vertex_attributes = attrs;
            pi.vertex_input_state.num_vertex_attributes = 1;
            pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pi.target_info.color_target_descriptions = &ct;
            pi.target_info.num_color_targets = 1;
            pi.multisample_state.sample_count = msaa_samples;
            pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

            solid = dev.create_pipeline(pi);
            LOG_INFO("  solid pipeline: {}", solid != nullptr ? "OK" : "FAILED");
        }

        // ---- Color pipeline (render to swapchain, alpha blend) ----
        {
            SDL_GPUColorTargetBlendState bs = alpha_blend(true);
            SDL_GPUColorTargetDescription ct{};
            ct.format = swapchain_format;
            ct.blend_state = bs;

            SDL_GPUVertexAttribute attrs[] = {
                { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0 },                          // position
                { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, sizeof(float) * 2 },     // color (4 bytes normalized)
            };
            SDL_GPUVertexBufferDescription vb{};
            vb.slot = 0;
            vb.pitch = sizeof(vertex_color);
            vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

            SDL_GPUGraphicsPipelineCreateInfo pi{};
            pi.vertex_shader = color_vs;
            pi.fragment_shader = common_fs;
            pi.vertex_input_state.vertex_buffer_descriptions = &vb;
            pi.vertex_input_state.num_vertex_buffers = 1;
            pi.vertex_input_state.vertex_attributes = attrs;
            pi.vertex_input_state.num_vertex_attributes = 2;
            pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pi.target_info.color_target_descriptions = &ct;
            pi.target_info.num_color_targets = 1;
            pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

            color = dev.create_pipeline(pi);
            LOG_INFO("  color pipeline: {}", color != nullptr ? "OK" : "FAILED");

            // Same but for line primitives
            pi.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
            color_lines = dev.create_pipeline(pi);
            LOG_INFO("  color_lines pipeline: {}", color_lines != nullptr ? "OK" : "FAILED");
        }

        // ---- Layer fill pipeline (render to MSAA RT, alpha blend) ----
        {
            SDL_GPUColorTargetBlendState bs = alpha_blend(true);
            SDL_GPUColorTargetDescription ct{};
            ct.format = rt_format;
            ct.blend_state = bs;

            SDL_GPUVertexAttribute attrs[] = {
                { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0 },                       // position
                { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT, sizeof(float) * 2 },         // entity_id
            };
            SDL_GPUVertexBufferDescription vb{};
            vb.slot = 0;
            vb.pitch = sizeof(vertex_entity);
            vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

            SDL_GPUGraphicsPipelineCreateInfo pi{};
            pi.vertex_shader = layer_vs;
            pi.fragment_shader = layer_fs;
            pi.vertex_input_state.vertex_buffer_descriptions = &vb;
            pi.vertex_input_state.num_vertex_buffers = 1;
            pi.vertex_input_state.vertex_attributes = attrs;
            pi.vertex_input_state.num_vertex_attributes = 2;
            pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pi.target_info.color_target_descriptions = &ct;
            pi.target_info.num_color_targets = 1;
            pi.target_info.has_depth_stencil_target = false;
            pi.multisample_state.sample_count = msaa_samples;
            pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

            layer_fill = dev.create_pipeline(pi);
            LOG_INFO("  layer_fill pipeline: {}", layer_fill != nullptr ? "OK" : "FAILED");
        }

        // ---- Blit pipeline (render to swapchain, premultiplied alpha) ----
        {
            SDL_GPUColorTargetBlendState bs = premultiplied_blend();
            SDL_GPUColorTargetDescription ct{};
            ct.format = swapchain_format;
            ct.blend_state = bs;

            SDL_GPUGraphicsPipelineCreateInfo pi{};
            pi.vertex_shader = blit_vs;
            pi.fragment_shader = blit_fs;
            pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pi.target_info.color_target_descriptions = &ct;
            pi.target_info.num_color_targets = 1;
            pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

            blit = dev.create_pipeline(pi);
            LOG_INFO("  blit pipeline: {}", blit != nullptr ? "OK" : "FAILED");
        }

        // ---- Selection blit pipeline (render to swapchain, alpha blend) ----
        {
            SDL_GPUColorTargetBlendState bs = alpha_blend(true);
            SDL_GPUColorTargetDescription ct{};
            ct.format = swapchain_format;
            ct.blend_state = bs;

            SDL_GPUGraphicsPipelineCreateInfo pi{};
            pi.vertex_shader = blit_vs;
            pi.fragment_shader = selection_fs;
            pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pi.target_info.color_target_descriptions = &ct;
            pi.target_info.num_color_targets = 1;
            pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

            selection = dev.create_pipeline(pi);
            LOG_INFO("  selection pipeline: {}", selection != nullptr ? "OK" : "FAILED");
        }

        // ---- Line2 pipeline (render to MSAA RT, additive blend) ----
        {
            SDL_GPUColorTargetBlendState bs = additive_blend();
            SDL_GPUColorTargetDescription ct{};
            ct.format = rt_format;
            ct.blend_state = bs;

            // Quad vertices only - instance data comes from storage buffers
            SDL_GPUVertexAttribute attrs[] = {
                { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0 },    // position (quad)
            };
            SDL_GPUVertexBufferDescription vb{};
            vb.slot = 0;
            vb.pitch = sizeof(vertex_solid);
            vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

            SDL_GPUGraphicsPipelineCreateInfo pi{};
            pi.vertex_shader = line2_vs;
            pi.fragment_shader = line2_fs;
            pi.vertex_input_state.vertex_buffer_descriptions = &vb;
            pi.vertex_input_state.num_vertex_buffers = 1;
            pi.vertex_input_state.vertex_attributes = attrs;
            pi.vertex_input_state.num_vertex_attributes = 1;
            pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
            pi.target_info.color_target_descriptions = &ct;
            pi.target_info.num_color_targets = 1;
            pi.multisample_state.sample_count = msaa_samples;
            pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

            line2 = dev.create_pipeline(pi);
            LOG_INFO("  line2 pipeline: {}", line2 != nullptr ? "OK" : "FAILED");
        }

        // Release shader objects (pipelines hold references)
        dev.release_shader(solid_vs);
        dev.release_shader(color_vs);
        dev.release_shader(common_fs);
        dev.release_shader(layer_vs);
        dev.release_shader(layer_fs);
        dev.release_shader(line2_vs);
        dev.release_shader(line2_fs);
        dev.release_shader(blit_vs);
        dev.release_shader(blit_fs);
        dev.release_shader(selection_fs);

        LOG_INFO("All pipelines created successfully");
        return true;
    }

    void pipelines::cleanup(device &dev)
    {
        dev.release_pipeline(solid);
        dev.release_pipeline(color);
        dev.release_pipeline(color_lines);
        dev.release_pipeline(layer_fill);
        dev.release_pipeline(blit);
        dev.release_pipeline(blit_blend_premultiplied);
        dev.release_pipeline(selection);
        dev.release_pipeline(selection_blend);
        dev.release_pipeline(line2);
        dev.release_pipeline(line2_additive);
        dev.release_sampler(nearest_sampler);
        solid = color = layer_fill = blit = blit_blend_premultiplied = nullptr;
        selection = selection_blend = line2 = line2_additive = nullptr;
        nearest_sampler = nullptr;
    }

    //////////////////////////////////////////////////////////////////////
    // Drawlist

    void drawlist::reset()
    {
        verts.clear();
        verts.reserve(max_verts);
        entries.clear();
    }

    void drawlist::add_vertex(vec2d const &pos, gpu::color c)
    {
        if(entries.empty() || verts.size() >= max_verts) {
            return;
        }
        verts.emplace_back(static_cast<float>(pos.x), static_cast<float>(pos.y), c);
        entries.back().count += 1;
    }

    void drawlist::lines()
    {
        entries.emplace_back(SDL_GPU_PRIMITIVETYPE_LINELIST, static_cast<uint32_t>(verts.size()), 0);
    }

    void drawlist::add_line(vec2d const &start, vec2d const &end, gpu::color c)
    {
        add_vertex(start, c);
        add_vertex(end, c);
    }

    void drawlist::add_outline_rect(rect const &r, gpu::color c)
    {
        // 4 individual lines (8 verts) instead of line strip
        entries.emplace_back(SDL_GPU_PRIMITIVETYPE_LINELIST, static_cast<uint32_t>(verts.size()), 0);
        add_vertex(r.min_pos, c);
        add_vertex({ r.max_pos.x, r.min_pos.y }, c);
        add_vertex({ r.max_pos.x, r.min_pos.y }, c);
        add_vertex(r.max_pos, c);
        add_vertex(r.max_pos, c);
        add_vertex({ r.min_pos.x, r.max_pos.y }, c);
        add_vertex({ r.min_pos.x, r.max_pos.y }, c);
        add_vertex(r.min_pos, c);
    }

    void drawlist::add_rect(rect const &r, gpu::color c)
    {
        // Two triangles (6 verts) instead of triangle fan/strip
        entries.emplace_back(SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, static_cast<uint32_t>(verts.size()), 0);
        add_vertex(r.min_pos, c);
        add_vertex({ r.max_pos.x, r.min_pos.y }, c);
        add_vertex(r.max_pos, c);
        add_vertex(r.min_pos, c);
        add_vertex(r.max_pos, c);
        add_vertex({ r.min_pos.x, r.max_pos.y }, c);
    }

}    // namespace gpu
