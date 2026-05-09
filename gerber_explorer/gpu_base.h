//////////////////////////////////////////////////////////////////////
// SDL_GPU rendering backend - parallel implementation to gl_base.h
// This file provides SDL_GPU equivalents of all GL rendering primitives.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "gerber_2d.h"
#include "gl_colors.h"

//////////////////////////////////////////////////////////////////////

namespace gpu
{
    //////////////////////////////////////////////////////////////////////
    // Vertex types (same layout as GL, shared with tesselation code)

    struct vertex_solid
    {
        float x, y;
    };

    struct vertex_color
    {
        float x, y;
        uint32_t color;
    };

    struct vertex_entity
    {
        float x, y;
        uint32_t entity_id;
    };

    //////////////////////////////////////////////////////////////////////
    // Line instance data (matches gl::line2_program::line)

    struct line_instance
    {
        uint32_t start_index;
        uint32_t end_index;
        uint32_t entity_id;
        uint32_t pad;
    };

    //////////////////////////////////////////////////////////////////////
    // Arc instance data (matches gl::arc_program::arc)

    struct arc_instance
    {
        gerber_lib::vec2f center;
        float radius;
        float start_angle;
        float sweep;
        gerber_lib::vec2f extent_min;
        gerber_lib::vec2f extent_max;
    };

    //////////////////////////////////////////////////////////////////////
    // GPU device wrapper

    struct device
    {
        SDL_GPUDevice *gpu{};
        SDL_Window *window{};

        bool init(SDL_Window *win);
        void shutdown();

        // Load pre-compiled SPIR-V and create GPU shader (SPIRV-Cross translates to backend)
        SDL_GPUShader *compile_shader(char const *spv_name, char const *entry_point, SDL_GPUShaderStage stage,
                                      int num_samplers, int num_storage_textures, int num_storage_buffers, int num_uniform_buffers);

        // Buffer creation
        SDL_GPUBuffer *create_buffer(SDL_GPUBufferUsageFlags usage, uint32_t size, char const *name = nullptr);
        SDL_GPUTransferBuffer *create_transfer_buffer(SDL_GPUTransferBufferUsage usage, uint32_t size);

        // Upload data to a GPU buffer
        void upload_to_buffer(SDL_GPUBuffer *buffer, void const *data, uint32_t size);

        // Texture creation
        SDL_GPUTexture *create_texture(uint32_t width, uint32_t height, SDL_GPUTextureFormat format,
                                       SDL_GPUTextureUsageFlags usage, SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1);

        // Pipeline creation helpers
        SDL_GPUGraphicsPipeline *create_pipeline(SDL_GPUGraphicsPipelineCreateInfo const &info);

        void release_buffer(SDL_GPUBuffer *buffer);
        void release_texture(SDL_GPUTexture *texture);
        void release_pipeline(SDL_GPUGraphicsPipeline *pipeline);
        void release_shader(SDL_GPUShader *shader);
        void release_sampler(SDL_GPUSampler *sampler);
    };

    //////////////////////////////////////////////////////////////////////
    // Render target (replaces gl::render_target)

    struct render_target
    {
        SDL_GPUTexture *color_texture{};
        SDL_GPUTexture *resolve_texture{};    // non-MSAA resolve target
        uint32_t width{};
        uint32_t height{};
        SDL_GPUSampleCount sample_count{SDL_GPU_SAMPLECOUNT_1};

        void init(device &dev, uint32_t w, uint32_t h, SDL_GPUSampleCount samples);
        void cleanup(device &dev);
    };

    //////////////////////////////////////////////////////////////////////
    // All pipelines and shared state for the rendering backend

    struct pipelines
    {
        // Pipelines (replace GL program objects)
        SDL_GPUGraphicsPipeline *solid{};
        SDL_GPUGraphicsPipeline *color{};
        SDL_GPUGraphicsPipeline *color_lines{};
        SDL_GPUGraphicsPipeline *layer_fill{};
        SDL_GPUGraphicsPipeline *blit{};
        SDL_GPUGraphicsPipeline *blit_blend_premultiplied{};
        SDL_GPUGraphicsPipeline *selection{};
        SDL_GPUGraphicsPipeline *selection_blend{};
        SDL_GPUGraphicsPipeline *line2{};
        SDL_GPUGraphicsPipeline *line2_additive{};

        // Sampler for MSAA texture reads
        SDL_GPUSampler *nearest_sampler{};

        bool init(device &dev, SDL_GPUTextureFormat swapchain_format, SDL_GPUSampleCount msaa_samples = SDL_GPU_SAMPLECOUNT_1);
        void cleanup(device &dev);
    };

    //////////////////////////////////////////////////////////////////////
    // Drawlist for overlay graphics (replaces gl::drawlist)

    struct drawlist
    {
        using rect = gerber_lib::rect;
        using vec2d = gerber_lib::vec2d;

        struct entry
        {
            SDL_GPUPrimitiveType primitive;
            uint32_t offset;
            uint32_t count;
        };

        static constexpr int max_verts = 8192;

        std::vector<vertex_color> verts;
        std::vector<entry> entries;

        void reset();

        void add_vertex(vec2d const &pos, gl::color color);

        void lines();
        void add_line(vec2d const &start, vec2d const &end, gl::color color);
        void add_outline_rect(rect const &r, gl::color color);
        void add_rect(rect const &r, gl::color color);
    };

}    // namespace gpu
