//////////////////////////////////////////////////////////////////////

#include "gpu_drawer.h"
#include "gerber_drawer.h"

LOG_CONTEXT("gpu_drawer", info);

namespace gerber
{
    void gpu_drawer_resources::create(gpu::device &dev, gerber_drawer const &drawer)
    {
        if(ready) {
            return;
        }

        release(dev);

        if(drawer.fill_vertices.empty() || drawer.fill_indices.empty()) {
            return;
        }

        LOG_DEBUG("Creating GPU resources for layer '{}'", drawer.name());

        // Fill geometry
        {
            uint32_t vb_size = static_cast<uint32_t>(drawer.fill_vertices.size() * sizeof(gpu::vertex_entity));
            vertex_buffer = dev.create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, vb_size, "fill_verts");
            dev.upload_to_buffer(vertex_buffer, drawer.fill_vertices.data(), vb_size);

            num_indices = static_cast<uint32_t>(drawer.fill_indices.size());
            uint32_t ib_size = num_indices * sizeof(uint32_t);
            index_buffer = dev.create_buffer(SDL_GPU_BUFFERUSAGE_INDEX, ib_size, "fill_indices");
            dev.upload_to_buffer(index_buffer, drawer.fill_indices.data(), ib_size);
        }

        // Flags storage buffer - pad uint8 to uint32 for storage buffer compatibility
        if(!drawer.entity_flags.empty()) {
            uint32_t count = static_cast<uint32_t>(drawer.entity_flags.size());
            std::vector<uint32_t> flags32(count);
            for(uint32_t i = 0; i < count; ++i) {
                flags32[i] = drawer.entity_flags[i];
            }
            uint32_t size = count * sizeof(uint32_t);
            flags_buffer = dev.create_buffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, size, "flags");
            dev.upload_to_buffer(flags_buffer, flags32.data(), size);
        }

        // Line instance storage buffer
        if(!drawer.outline_lines.empty()) {
            num_lines = static_cast<uint32_t>(drawer.outline_lines.size());
            uint32_t size = num_lines * sizeof(gpu::line_instance);
            line_instance_buffer = dev.create_buffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, size, "line_instances");
            dev.upload_to_buffer(line_instance_buffer, drawer.outline_lines.data(), size);
        }

        // Line vertex storage buffer
        if(!drawer.outline_vertices.empty()) {
            uint32_t size = static_cast<uint32_t>(drawer.outline_vertices.size() * sizeof(gerber_lib::vec2f));
            line_vertex_buffer = dev.create_buffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, size, "line_verts");
            dev.upload_to_buffer(line_vertex_buffer, drawer.outline_vertices.data(), size);
        }

        // Mask geometry (for inverted layers)
        if(drawer.got_mask && !drawer.mask.vertices.empty() && !drawer.mask.indices.empty()) {
            uint32_t mvb_size = static_cast<uint32_t>(drawer.mask.vertices.size() * sizeof(gpu::vertex_solid));
            mask_vertex_buffer = dev.create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, mvb_size, "mask_verts");
            dev.upload_to_buffer(mask_vertex_buffer, drawer.mask.vertices.data(), mvb_size);

            mask_num_indices = static_cast<uint32_t>(drawer.mask.indices.size());
            uint32_t mib_size = mask_num_indices * sizeof(uint32_t);
            mask_index_buffer = dev.create_buffer(SDL_GPU_BUFFERUSAGE_INDEX, mib_size, "mask_indices");
            dev.upload_to_buffer(mask_index_buffer, drawer.mask.indices.data(), mib_size);
        }

        ready = true;
        LOG_DEBUG("GPU resources created: {} verts, {} indices, {} lines",
                 drawer.fill_vertices.size(), drawer.fill_indices.size(), num_lines);
    }

    void gpu_drawer_resources::release(gpu::device &dev)
    {
        dev.release_buffer(vertex_buffer);
        dev.release_buffer(index_buffer);
        dev.release_buffer(flags_buffer);
        dev.release_buffer(line_instance_buffer);
        dev.release_buffer(line_vertex_buffer);
        dev.release_buffer(mask_vertex_buffer);
        dev.release_buffer(mask_index_buffer);
        vertex_buffer = index_buffer = flags_buffer = nullptr;
        line_instance_buffer = line_vertex_buffer = nullptr;
        mask_vertex_buffer = mask_index_buffer = nullptr;
        num_indices = num_lines = mask_num_indices = 0;
        ready = false;
    }

    void gpu_drawer_resources::update_flags(gpu::device &dev, gerber_drawer const &drawer)
    {
        if(!flags_buffer || drawer.entity_flags.empty()) {
            return;
        }
        uint32_t count = static_cast<uint32_t>(drawer.entity_flags.size());
        std::vector<uint32_t> flags32(count);
        for(uint32_t i = 0; i < count; ++i) {
            flags32[i] = drawer.entity_flags[i];
        }
        uint32_t size = count * sizeof(uint32_t);
        dev.upload_to_buffer(flags_buffer, flags32.data(), size);
    }

}    // namespace gerber
