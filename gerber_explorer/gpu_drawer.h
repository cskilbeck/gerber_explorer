//////////////////////////////////////////////////////////////////////
// SDL_GPU drawer - manages GPU buffers for a single gerber layer
// Parallel to the GL parts of gl_drawer. Shares CPU-side tesselation data.

#pragma once

#include "gpu_base.h"
#include "gerber_arena.h"

namespace gerber
{
    struct gl_drawer;    // forward - we read its arenas

    struct gpu_drawer_resources
    {
        // GPU buffers for fill geometry
        SDL_GPUBuffer *vertex_buffer{};      // vertex_entity data
        SDL_GPUBuffer *index_buffer{};       // uint32 triangle indices
        uint32_t num_indices{};

        // Storage buffers (replace GL TBOs)
        SDL_GPUBuffer *flags_buffer{};           // uint32 per entity (padded from uint8)
        SDL_GPUBuffer *line_instance_buffer{};   // line_instance structs
        SDL_GPUBuffer *line_vertex_buffer{};     // vec2f positions
        uint32_t num_lines{};

        // Mask for inverted layers
        SDL_GPUBuffer *mask_vertex_buffer{};
        SDL_GPUBuffer *mask_index_buffer{};
        uint32_t mask_num_indices{};

        bool ready{};

        void create(gpu::device &dev, gl_drawer const &drawer);
        void release(gpu::device &dev);
        void update_flags(gpu::device &dev, gl_drawer const &drawer);
    };

}    // namespace gerber
