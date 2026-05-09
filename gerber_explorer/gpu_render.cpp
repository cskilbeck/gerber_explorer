//////////////////////////////////////////////////////////////////////
// SDL_GPU rendering path for gerber_explorer
// This file contains the gpu_render() function and helpers.
// It parallels the GL rendering in on_render() (gerber_explorer.cpp).

#include <glad/glad.h>    // needed because gerber_explorer.h pulls gl_base.h

#include "gerber_explorer.h"
#include "gerber_net.h"
#include "gl_matrix.h"
#include "imgui.h"
#include "imgui_impl_sdlgpu3.h"

LOG_CONTEXT("gpu_render", info);

using namespace gerber;

using rect = gerber_lib::rect;
using vec2d = gerber_lib::vec2d;

static SDL_GPUSampleCount msaa_int_to_enum(int n)
{
    if(n >= 8) return SDL_GPU_SAMPLECOUNT_8;
    if(n >= 4) return SDL_GPU_SAMPLECOUNT_4;
    if(n >= 2) return SDL_GPU_SAMPLECOUNT_2;
    return SDL_GPU_SAMPLECOUNT_1;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::gpu_render()
{
    gpu_cmd = SDL_AcquireGPUCommandBuffer(gpu_dev.gpu);
    if(!gpu_cmd) {
        LOG_ERROR("Failed to acquire command buffer: {}", SDL_GetError());
        return;
    }

    // Acquire swapchain texture (shared with on_gpu_imgui)
    gpu_swapchain_texture = nullptr;
    uint32_t sw_w, sw_h;
    if(!SDL_AcquireGPUSwapchainTexture(gpu_cmd, window, &gpu_swapchain_texture, &sw_w, &sw_h)) {
        SDL_SubmitGPUCommandBuffer(gpu_cmd);
        gpu_cmd = nullptr;
        return;
    }
    if(!gpu_swapchain_texture) {
        SDL_SubmitGPUCommandBuffer(gpu_cmd);
        gpu_cmd = nullptr;
        return;
    }

    SDL_GPUCommandBuffer *cmd = gpu_cmd;
    SDL_GPUTexture *swapchain_texture = gpu_swapchain_texture;

    // Compute display scale (logical pixels to physical pixels)
    float display_scale = SDL_GetWindowDisplayScale(window);
    if(display_scale <= 0) display_scale = 1.0f;

    if(frames < 5) {
        int win_w, win_h, pix_w, pix_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_GetWindowSizeInPixels(window, &pix_w, &pix_h);
        LOG_INFO("Frame {}: window={}x{}, pixels={}x{}, swapchain={}x{}, viewport=({},{} {}x{}), scale={}, window_size=({},{})",
                 frames, win_w, win_h, pix_w, pix_h, sw_w, sw_h,
                 viewport_xpos, viewport_ypos, viewport_width, viewport_height,
                 display_scale, window_width, window_height);
    }

    // Recreate pipelines if MSAA setting changed
    SDL_GPUSampleCount samples = msaa_int_to_enum(settings.multisamples);
    if(settings.multisamples != gpu_current_msaa) {
        gpu_current_msaa = settings.multisamples;
        SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(gpu_dev.gpu, window);
        gpu_pipelines.cleanup(gpu_dev);
        gpu_pipelines.init(gpu_dev, swapchain_format, samples);
        gpu_render_target.cleanup(gpu_dev);    // force RT recreation with new sample count
    }

    // Resize render target to match viewport dimensions
    uint32_t rt_w = static_cast<uint32_t>(viewport_width * display_scale);
    uint32_t rt_h = static_cast<uint32_t>(viewport_height * display_scale);

    if(rt_w > 0 && rt_h > 0 && (gpu_render_target.width != rt_w || gpu_render_target.height != rt_h)) {
        gpu_render_target.init(gpu_dev, rt_w, rt_h, samples);
    }

    // Build ordered layer list (same logic as GL path - already done in on_render before we get here)

    // ---- Clear swapchain to background color ----
    {
        SDL_GPUColorTargetInfo ct{};
        ct.texture = swapchain_texture;
        ct.load_op = SDL_GPU_LOADOP_CLEAR;
        ct.store_op = SDL_GPU_STOREOP_STORE;
        ct.clear_color.r = settings.background_color.r;
        ct.clear_color.g = settings.background_color.g;
        ct.clear_color.b = settings.background_color.b;
        ct.clear_color.a = 1.0f;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
        SDL_EndGPURenderPass(pass);
    }

    // ---- Render each layer ----
    gerber_layer *outline_layer = get_outline_layer();

    std::vector<gerber_layer *> ordered_layers;
    for(auto *l : layers) {
        if(layer_is_visible(l) && l->is_valid()) {
            ordered_layers.push_back(l);
        }
    }

    for(auto *layer_ptr : ordered_layers) {
        gerber_layer &layer = *layer_ptr;

        // Ensure GPU resources exist
        layer.gpu_resources.create(gpu_dev, *layer.drawer);
        if(!layer.gpu_resources.ready) {
            continue;
        }

        // Sync entity flags CPU-side (without GL upload), then upload to GPU buffer
        {
            bool flags_changed = false;
            for(auto const &e : layer.drawer->entities) {
                int id = e.entity_id();
                if(id >= 0 && id < (int)layer.drawer->entity_flags.size()) {
                    if(layer.drawer->entity_flags[id] != (uint8_t)e.flags) {
                        layer.drawer->entity_flags[id] = (uint8_t)e.flags;
                        flags_changed = true;
                    }
                }
            }
            if(flags_changed) {
                layer.gpu_resources.update_flags(gpu_dev, *layer.drawer);
            }
        }

        gpu_render_layer(cmd, layer, outline_layer);

        // Blit resolved RT to swapchain
        if(frames < 5) {
            LOG_INFO("Blit: RT={}x{}, viewport=({},{} {}x{}), swapchain={}x{}",
                     gpu_render_target.width, gpu_render_target.height,
                     viewport_xpos, viewport_ypos, viewport_width, viewport_height, sw_w, sw_h);
        }
        if(gpu_render_target.color_texture) {
            SDL_GPUColorTargetInfo blit_ct{};
            blit_ct.texture = swapchain_texture;
            blit_ct.load_op = SDL_GPU_LOADOP_LOAD;
            blit_ct.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass *blit_pass = SDL_BeginGPURenderPass(cmd, &blit_ct, 1, nullptr);
            SDL_GPUViewport blit_vp{ (float)viewport_xpos, (float)viewport_ypos,
                                     (float)viewport_width, (float)viewport_height, 0, 1 };
            SDL_SetGPUViewport(blit_pass, &blit_vp);
            SDL_BindGPUGraphicsPipeline(blit_pass, gpu_pipelines.blit);

            // Blit fragment uniforms: fill_color + other_color
            struct {
                float fill_color[4];
                float other_color[4];
            } blit_uniforms;
            gl::colorf4 fc(layer.fill_color);
            gl::colorf4 oc(gl::colors::black);
            memcpy(blit_uniforms.fill_color, fc.f, sizeof(float) * 4);
            memcpy(blit_uniforms.other_color, oc.f, sizeof(float) * 4);
            SDL_PushGPUFragmentUniformData(cmd, 0, &blit_uniforms, sizeof(blit_uniforms));

            // Bind resolved (or color if no MSAA) texture as sampler input
            SDL_GPUTextureSamplerBinding tsb{};
            tsb.texture = (gpu_render_target.sample_count != SDL_GPU_SAMPLECOUNT_1)
                            ? gpu_render_target.resolve_texture
                            : gpu_render_target.color_texture;
            tsb.sampler = gpu_pipelines.nearest_sampler;
            SDL_BindGPUFragmentSamplers(blit_pass, 0, &tsb, 1);

            SDL_DrawGPUPrimitives(blit_pass, 3, 1, 0, 0);

            SDL_EndGPURenderPass(blit_pass);
        }
    }

    // ---- Selection overlay ----
    if(selected_layer != nullptr && !selected_layer->drawer->entities.empty() && selected_layer->gpu_resources.ready) {
        gpu_render_selection(cmd);
    }

    // ---- Overlay graphics (axes, selection rect, measure line) ----
    gpu_render_overlay(cmd, swapchain_texture);

    // Don't submit yet — on_gpu_imgui() will render ImGui and submit
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::gpu_render_layer(SDL_GPUCommandBuffer *cmd, gerber_layer &layer, gerber_layer *outline_layer)
{
    auto &res = layer.gpu_resources;
    auto &rt = gpu_render_target;

    if(!rt.color_texture) return;

    uint8_t fill_flag = entity_flags_t::fill;
    uint8_t clear_flag = entity_flags_t::clear;

    // ---- Render to MSAA render target ----
    {
        SDL_GPUColorTargetInfo ct{};
        ct.texture = rt.color_texture;
        ct.load_op = SDL_GPU_LOADOP_CLEAR;
        ct.clear_color = { 0, 0, 0, 0 };
        if(rt.sample_count != SDL_GPU_SAMPLECOUNT_1) {
            ct.store_op = SDL_GPU_STOREOP_RESOLVE_AND_STORE;
            ct.resolve_texture = rt.resolve_texture;
        } else {
            ct.store_op = SDL_GPU_STOREOP_STORE;
        }

        // For inverted layers, clear to red (fill) or draw mask in red
        if(layer.invert) {
            if(outline_layer == nullptr) {
                ct.clear_color = { 1, 0, 0, 0 };
            }
            std::swap(fill_flag, clear_flag);
        }

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
        // Default viewport matches render target dimensions

        // If inverted with outline mask, draw the mask in red first
        if(layer.invert && outline_layer != nullptr && outline_layer->gpu_resources.mask_vertex_buffer) {
            auto &mask_res = outline_layer->gpu_resources;
            SDL_BindGPUGraphicsPipeline(pass, gpu_pipelines.solid);

            // Push uniforms: transform + color
            struct {
                gl::matrix transform;
                float color[4];
            } solid_uniforms;
            solid_uniforms.transform = world_matrix;
            solid_uniforms.color[0] = 1;
            solid_uniforms.color[1] = 0;
            solid_uniforms.color[2] = 0;
            solid_uniforms.color[3] = 1;
            SDL_PushGPUVertexUniformData(cmd, 0, &solid_uniforms, sizeof(solid_uniforms));

            SDL_GPUBufferBinding vb_bind{};
            vb_bind.buffer = mask_res.mask_vertex_buffer;
            SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);

            SDL_GPUBufferBinding ib_bind{};
            ib_bind.buffer = mask_res.mask_index_buffer;
            SDL_BindGPUIndexBuffer(pass, &ib_bind, SDL_GPU_INDEXELEMENTSIZE_32BIT);

            SDL_DrawGPUIndexedPrimitives(pass, mask_res.mask_num_indices, 1, 0, 0, 0);
        }

        // Draw layer fill
        SDL_BindGPUGraphicsPipeline(pass, gpu_pipelines.layer_fill);

        // Vertex uniforms: transform + draw_flags
        struct {
            gl::matrix transform;
            int32_t draw_flags;
            int32_t _pad[3];
        } layer_vs_uniforms;
        layer_vs_uniforms.transform = world_matrix;
        layer_vs_uniforms.draw_flags = entity_flags_t::fill | entity_flags_t::clear;
        SDL_PushGPUVertexUniformData(cmd, 0, &layer_vs_uniforms, sizeof(layer_vs_uniforms));

        // Fragment uniforms: red/green/blue flags + value
        struct {
            int32_t red_flags;
            int32_t green_flags;
            int32_t blue_flags;
            int32_t _pad;
            float value[4];
        } layer_fs_uniforms;
        layer_fs_uniforms.red_flags = fill_flag;
        layer_fs_uniforms.green_flags = clear_flag;
        layer_fs_uniforms.blue_flags = 0;
        layer_fs_uniforms._pad = 0;
        layer_fs_uniforms.value[0] = 1;
        layer_fs_uniforms.value[1] = 1;
        layer_fs_uniforms.value[2] = 1;
        layer_fs_uniforms.value[3] = 1;
        SDL_PushGPUFragmentUniformData(cmd, 0, &layer_fs_uniforms, sizeof(layer_fs_uniforms));

        // Bind vertex storage buffer (flags)
        SDL_BindGPUVertexStorageBuffers(pass, 0, &res.flags_buffer, 1);

        // Bind vertex/index buffers
        SDL_GPUBufferBinding vb{};
        vb.buffer = res.vertex_buffer;
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

        SDL_GPUBufferBinding ib{};
        ib.buffer = res.index_buffer;
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        SDL_DrawGPUIndexedPrimitives(pass, res.num_indices, 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

    // Blit is handled by the caller (gpu_render) after this function returns
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::gpu_render_selection(SDL_GPUCommandBuffer *cmd)
{
    // TODO: implement selection rendering
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::gpu_render_overlay(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain_texture)
{
    // Build overlay drawlist (same as GL path)
    gpu_overlay.reset();

    if(mouse_mode == mouse_drag_zoom_select) {
        // TODO: correct_aspect_ratio for zoom select preview
        gpu_overlay.add_rect(drag_rect, 0x800000ff);
        gpu_overlay.add_outline_rect(drag_rect, 0xffffffff);
    }

    vec2d origin = viewport_pos_from_world_pos({ 0, 0 });

    if(settings.show_axes) {
        gpu_overlay.lines();
        gpu_overlay.add_line({ 0, origin.y }, { viewport_size.x, origin.y }, gl::colors::cyan);
        gpu_overlay.add_line({ origin.x, 0 }, { origin.x, viewport_size.y }, gl::colors::cyan);
    }

    if(settings.show_extent && selected_layer != nullptr && selected_layer->is_valid()) {
        if(active_entity != nullptr) {
            rect s = viewport_rect_from_board_rect(active_entity->bounds);
            gpu_overlay.add_outline_rect(s, gl::colors::yellow);
        } else {
            rect ext = selected_layer->extent();
            if(ext.width() != 0 && ext.height() != 0) {
                rect s{ viewport_pos_from_world_pos(ext.min_pos), viewport_pos_from_world_pos(ext.max_pos) };
                gpu_overlay.add_outline_rect(s, gl::colors::yellow);
            }
        }
    }

    if(mouse_mode == mouse_drag_select) {
        rect f{ drag_mouse_start_pos, drag_mouse_cur_pos };
        uint32_t c = 0x40ff8020;
        if(f.min_pos.x > f.max_pos.x) {
            c = 0x4080ff20;
        }
        gpu_overlay.add_rect(f, c);
        gpu_overlay.add_outline_rect(f, 0xffffffff);
    }

    if(measure_dragging || measure_line_visible) {
        vec2d start_screen = viewport_pos_from_world_pos(measure_start_world);
        vec2d end_screen = viewport_pos_from_world_pos(measure_end_world);
        gpu_overlay.lines();
        gpu_overlay.add_line(start_screen, end_screen, gl::colors::yellow);
    }

    if(gpu_overlay.verts.empty()) {
        return;
    }

    // Upload overlay verts
    uint32_t vb_size = static_cast<uint32_t>(gpu_overlay.verts.size() * sizeof(gpu::vertex_color));

    // Recreate overlay VBO if needed
    if(gpu_overlay_vbo) {
        gpu_dev.release_buffer(gpu_overlay_vbo);
    }
    gpu_overlay_vbo = gpu_dev.create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, vb_size);
    gpu_dev.upload_to_buffer(gpu_overlay_vbo, gpu_overlay.verts.data(), vb_size);

    // Render overlay to swapchain
    SDL_GPUColorTargetInfo ct{};
    ct.texture = swapchain_texture;
    ct.load_op = SDL_GPU_LOADOP_LOAD;
    ct.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
    float display_scale = SDL_GetWindowDisplayScale(window);
    if(display_scale <= 0) display_scale = 1.0f;
    SDL_GPUViewport vp_overlay{ viewport_xpos * display_scale, viewport_ypos * display_scale,
                                viewport_width * display_scale, viewport_height * display_scale, 0, 1 };
    SDL_SetGPUViewport(pass, &vp_overlay);

    SDL_GPUBufferBinding vb{};
    vb.buffer = gpu_overlay_vbo;

    SDL_GPUPrimitiveType current_primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    SDL_BindGPUGraphicsPipeline(pass, gpu_pipelines.color);
    SDL_PushGPUVertexUniformData(cmd, 0, &ortho_screen_matrix, sizeof(ortho_screen_matrix));
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

    for(auto const &entry : gpu_overlay.entries) {
        if(entry.count > 0) {
            if(entry.primitive != current_primitive) {
                current_primitive = entry.primitive;
                SDL_BindGPUGraphicsPipeline(pass, (current_primitive == SDL_GPU_PRIMITIVETYPE_LINELIST)
                                                      ? gpu_pipelines.color_lines
                                                      : gpu_pipelines.color);
                SDL_PushGPUVertexUniformData(cmd, 0, &ortho_screen_matrix, sizeof(ortho_screen_matrix));
                SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
            }
            SDL_DrawGPUPrimitives(pass, entry.count, 1, entry.offset, 0);
        }
    }

    SDL_EndGPURenderPass(pass);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_gpu_imgui()
{
    if(!gpu_cmd) {
        // gpu_render() failed to acquire — nothing to do
        return;
    }

    ImDrawData *draw_data = ImGui::GetDrawData();
    if(draw_data) {
        // Must call PrepareDrawData BEFORE the render pass to upload vertex/index buffers
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, gpu_cmd);

        if(gpu_swapchain_texture) {
            SDL_GPUColorTargetInfo ct{};
            ct.texture = gpu_swapchain_texture;
            ct.load_op = SDL_GPU_LOADOP_LOAD;
            ct.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(gpu_cmd, &ct, 1, nullptr);
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, gpu_cmd, pass);
            SDL_EndGPURenderPass(pass);
        }
    }

    SDL_SubmitGPUCommandBuffer(gpu_cmd);
    gpu_cmd = nullptr;
    gpu_swapchain_texture = nullptr;
}
