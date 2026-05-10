#if defined(_WIN32)
#include <windows.h>
#endif

#include <filesystem>
#include <expected>

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <nfd.h>

#include "gerber_explorer.h"
#include "gerber_util.h"

#include "gerber_aperture.h"
#include "gerber_net.h"
#include "gpu_3d_drawer.h"
#include "gpu_matrix.h"
#include "gpu_colors.h"
#include "util.h"

#include "assets/matsym_codepoints_utf8.h"

using namespace sdl_compat;

LOG_CONTEXT("gerber_explorer", info);

namespace
{
    using namespace gerber;



    char const *const board_view_names[board_view_num_views] = { "All", "Front", "Back" };

    using gerber_lib::rect;
    using gerber_lib::vec2d;
    using gerber_lib::vec2f;

    long long const zoom_lerp_time_ms = 700;

    double const drag_select_offset_start_distance = 16;

    gpu::color layer_colors[] = { (gpu::color)gpu::colors::dark_green, (gpu::color)gpu::colors::dark_cyan,     (gpu::color)gpu::colors::green,
                                 (gpu::color)gpu::colors::lime_green, (gpu::color)gpu::colors::antique_white, (gpu::color)gpu::colors::corn_flower_blue,
                                 (gpu::color)gpu::colors::gold };

    size_t next_layer_color{ 0 };

    int next_index{ 1 };

    //////////////////////////////////////////////////////////////////////
    // make a rectangle have a certain aspect ratio by shrinking or expanding it

    enum aspect_ratio_correction
    {
        aspect_shrink,
        aspect_expand,
    };

    //////////////////////////////////////////////////////////////////////

    rect correct_aspect_ratio(double new_aspect_ratio, rect const &r, aspect_ratio_correction correction)
    {
        bool dir = r.aspect_ratio() > new_aspect_ratio;
        if(correction == aspect_expand) {
            dir = !dir;
        }
        vec2d n = r.size().scale(0.5);
        if(dir) {
            n.x = n.y * new_aspect_ratio;
        } else {
            n.y = n.x / new_aspect_ratio;
        }
        vec2d center = r.mid_point();
        return rect{ center.subtract(n), center.add(n) };
    }

    //////////////////////////////////////////////////////////////////////

    struct layer_defaults_t
    {
        gerber_lib::layer::type_t layer_type;
        bool is_inverted;
        layer_order_t layer_order;
        gpu::color color;
    };

    //////////////////////////////////////////////////////////////////////

    bool constexpr layer_normal = false;
    bool constexpr layer_inverted = true;

    layer_defaults_t layer_defaults[] = {
        { gerber_lib::layer::unknown, layer_normal, layer_order_t::other, gpu::colors::yellow },
        { gerber_lib::layer::vcut, layer_normal, layer_order_t::other, gpu::colors::magenta },
        { gerber_lib::layer::board, layer_normal, layer_order_t::other, gpu::colors::black },
        { gerber_lib::layer::outline, layer_normal, layer_order_t::other, gpu::colors::black },
        { gerber_lib::layer::mechanical, layer_normal, layer_order_t::other, gpu::colors::cyan },
        { gerber_lib::layer::info, layer_normal, layer_order_t::other, gpu::colors::white },
        { gerber_lib::layer::keepout, layer_normal, layer_order_t::other, gpu::colors::magenta },
        { gerber_lib::layer::drill, layer_normal, layer_order_t::drill, gpu::colors::black },
        { gerber_lib::layer::pads_top, layer_normal, layer_order_t::top_pads, gpu::colors::silver },
        { gerber_lib::layer::paste_top, layer_normal, layer_order_t::top_outer, gpu::colors::silver },
        { gerber_lib::layer::overlay_top, layer_normal, layer_order_t::top_outer, gpu::colors::white },
        { gerber_lib::layer::soldermask_top, layer_inverted, layer_order_t::top_outer, gpu::set_alpha(gpu::colors::dark_green, 0.75f) },
        { gerber_lib::layer::copper_top, layer_normal, layer_order_t::top_copper, 0xFF34AAAC },
        { gerber_lib::layer::copper_inner, layer_normal, layer_order_t::inner_copper, 0xFF34AAAC },
        { gerber_lib::layer::copper_bottom, layer_normal, layer_order_t::bottom_copper, 0xFF34AAAC },
        { gerber_lib::layer::soldermask_bottom, layer_inverted, layer_order_t::bottom_outer, gpu::set_alpha(gpu::colors::dark_green, 0.75f) },
        { gerber_lib::layer::overlay_bottom, layer_normal, layer_order_t::bottom_outer, gpu::colors::white },
        { gerber_lib::layer::paste_bottom, layer_normal, layer_order_t::bottom_outer, gpu::colors::silver },
        { gerber_lib::layer::pads_bottom, layer_normal, layer_order_t::bottom_pads, gpu::colors::silver },
    };

    //////////////////////////////////////////////////////////////////////

    bool is_bottom_layer(layer_order_t o)
    {
        return o == layer_order_t::bottom_outer || o == layer_order_t::bottom_copper || o == layer_order_t::drill || o == layer_order_t::bottom_pads;
    }

    //////////////////////////////////////////////////////////////////////

    bool is_top_layer(layer_order_t o)
    {
        return o == layer_order_t::top_outer || o == layer_order_t::top_copper || o == layer_order_t::drill || o == layer_order_t::top_pads;
    }

    //////////////////////////////////////////////////////////////////////

    layer_defaults_t get_defaults_for_layer_type(gerber_lib::layer::type_t layer_type)
    {
        layer_defaults_t const *p = nullptr;
        for(auto const &d : layer_defaults) {
            if(layer_type >= d.layer_type) {
                p = &d;
            }
        }
        return *p;
    }

}    // namespace

// GL program statics removed - GPU path only

//////////////////////////////////////////////////////////////////////
// If zoom_anim or any jobs in the pool, or user interacting, it's not idle
// But also... suppress idleness for a short while

void gerber_explorer::set_active()
{
    idle_timestamp = get_time();
}

bool gerber_explorer::is_idle()
{
    // poll events for this much time after last call to set_active()
    double constexpr idle_timer = 0.1f;
    return get_time() - idle_timestamp > idle_timer;
}

//////////////////////////////////////////////////////////////////////

std::string gerber_explorer::window_name() const
{
    return app_friendly_name;
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::world_pos_from_viewport_pos(vec2d const &p) const
{
    return vec2d{ p.x, p.y }.divide(view_scale).add(view_rect.min_pos);
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::world_rect_from_viewport_rect(rect const &r) const
{
    vec2d min = world_pos_from_viewport_pos(r.min_pos);
    vec2d max = world_pos_from_viewport_pos(r.max_pos);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::viewport_pos_from_world_pos(vec2d const &p) const
{
    return p.subtract(view_rect.min_pos).multiply(view_scale);
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::viewport_pos_from_board_pos(vec2d const &p) const
{
    vec2d b = board_pos_from_world_pos(p);
    return b.subtract(view_rect.min_pos).multiply(view_scale);
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::viewport_rect_from_world_rect(rect const &r) const
{
    vec2d min = viewport_pos_from_world_pos(r.min_pos);
    vec2d max = viewport_pos_from_world_pos(r.max_pos);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::viewport_rect_from_board_rect(rect const &r) const
{
    vec2d min = viewport_pos_from_board_pos(r.min_pos);
    vec2d max = viewport_pos_from_board_pos(r.max_pos);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::board_pos_from_viewport_pos(vec2d const &p) const
{
    vec2d pos = world_pos_from_viewport_pos(p);
    return pos.subtract(board_center).multiply(flip_xy).add(board_center);
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::board_rect_from_viewport_rect(rect const &r) const
{
    vec2d min = world_pos_from_viewport_pos(r.min_pos);
    vec2d max = world_pos_from_viewport_pos(r.max_pos);
    min = min.subtract(board_center).multiply(flip_xy).add(board_center);
    max = max.subtract(board_center).multiply(flip_xy).add(board_center);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::board_pos_from_world_pos(vec2d const &p) const
{
    return p.subtract(board_center).multiply(flip_xy).add(board_center);
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::board_rect_from_world_rect(rect const &r) const
{
    vec2d min = board_pos_from_world_pos(r.min_pos);
    vec2d max = board_pos_from_world_pos(r.max_pos);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::fit_to_viewport()
{
    if(active_entity != nullptr) {
        zoom_to_rect(board_rect_from_world_rect(active_entity->bounds));
    } else if(selected_layer != nullptr && selected_layer->is_valid() && selected_layer->extent().is_normalized()) {
        // zoom to selected entities or the whole layer
        rect extent{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
        int num_selected = 0;
        for(auto const &e : selected_layer->drawer->entities) {
            if((e.flags & entity_flags_t::selected) != 0) {
                extent = extent.union_with(e.bounds);
                num_selected += 1;
            }
        }
        if(num_selected == 0) {
            extent = selected_layer->extent();
        }
        zoom_to_rect(board_rect_from_world_rect(extent));
    } else if(!layers.empty()) {
        should_fit_to_viewport = true;
        update_board_extent();
        zoom_to_rect(visible_board_extent);
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::zoom_to_rect(rect const &zoom_rect, double border_ratio)
{
    if(viewport_width == 0 || viewport_height == 0) {
        return;
    }
    double aspect_ratio = (double)viewport_width / viewport_height;
    rect new_rect = correct_aspect_ratio(aspect_ratio, zoom_rect, aspect_expand);
    vec2d mid = new_rect.mid_point();
    vec2d siz = new_rect.size().scale(border_ratio / 2);
    target_view_rect = { mid.subtract(siz), mid.add(siz) };
    source_view_rect = view_rect;
    target_view_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(zoom_lerp_time_ms);
    zoom_anim = true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::zoom_at_point(vec2d const &zoom_pos, double zoom_scale)
{
    double const min_dimension = 0.00254;    // min view w/h is 2.54 microns (= 0.0001 inches = 0.1 thousandths)
    double const max_dimension = 10000.0;    // max view w/h is 10 meters

    vec2d bl = view_rect.min_pos.subtract(zoom_pos).multiply(zoom_scale).add(zoom_pos);
    vec2d tr = view_rect.max_pos.subtract(zoom_pos).multiply(zoom_scale).add(zoom_pos);

    vec2d size = tr.subtract(bl);

    double aspect_ratio = view_rect.width() / view_rect.height();

    if(size.x < min_dimension) {
        size.y = min_dimension / aspect_ratio;
    } else if(size.x > max_dimension) {
        size.y = max_dimension / aspect_ratio;
    } else if(size.y < min_dimension) {
        size.y = min_dimension;
    } else if(size.y > max_dimension) {
        size.y = max_dimension;
    }
    size.x = size.y * aspect_ratio;

    vec2d ratio = zoom_pos.subtract(view_rect.min_pos).divide(view_rect.size());
    bl = zoom_pos.subtract(size.multiply(ratio));
    tr = bl.add(size);
    view_rect = { bl, tr };
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::update_view_rect()
{
    if(zoom_anim) {

        set_active();

        auto lerp = [](double d) {
            double p = 10;
            double x = d;
            double x2 = pow(x, p - 1);
            double x1 = x2 * x;    // pow(x, p)
            return 1 - (p * x2 - (p - 1) * x1);
        };

        auto now = std::chrono::high_resolution_clock::now();
        auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(target_view_time - now).count();
        if(remaining_ms < 1) {
            remaining_ms = 0;
        }
        double t = (double)remaining_ms / (double)zoom_lerp_time_ms;
        double d = lerp(t);
        vec2d dmin = target_view_rect.min_pos.subtract(source_view_rect.min_pos).scale(d);
        vec2d dmax = target_view_rect.max_pos.subtract(source_view_rect.max_pos).scale(d);
        view_rect = { source_view_rect.min_pos.add(dmin), source_view_rect.max_pos.add(dmax) };
        vec2d wv = viewport_pos_from_world_pos(view_rect.min_pos);
        vec2d tv = viewport_pos_from_world_pos(target_view_rect.min_pos);
        if(wv.subtract(tv).length() <= 1) {
            view_rect = target_view_rect;
            zoom_anim = false;
        }
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_drop(int count, const char **paths)
{
    for(int i = 0; i < count; ++i) {
        add_gerber(settings::layer_t{ paths[i], "#ff00ff00", true, false, -1 });
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::next_view()
{
    settings.board_view += 1;
    if(settings.board_view >= board_view_num_views) {
        settings.board_view = board_view_all;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_key(int key, int scancode, int action, int mods)
{
    set_active();
    if(action == ACTION_PRESS) {
        if(mods == 0) {
            switch(key) {
            case KEY_ESCAPE:
                set_should_close();
                break;
            case KEY_V:
                next_view();
                break;
            case KEY_F:
                fit_to_viewport();
                break;
            case KEY_X:
                settings.flip_x = !settings.flip_x;
                break;
            case KEY_Y:
                settings.flip_y = !settings.flip_y;
                break;
            case KEY_W:
                settings.wireframe = !settings.wireframe;
                break;
            case KEY_A:
                settings.show_axes = !settings.show_axes;
                break;
            case KEY_E:
                settings.show_extent = !settings.show_extent;
                break;
            default:
                break;
            }
        } else if(mods & KMOD_CTRL_FLAG) {
            switch(key) {
            case KEY_O:
                file_open();
                break;
            case KEY_S: {
                auto save_path = save_file_dialog("settings.json");
                if(save_path.has_value()) {
                    save_settings(save_path.value(), true);
                }
            } break;
            case KEY_L: {
                auto load_path = load_file_dialog();
                if(load_path.has_value()) {
                    load_settings(load_path.value());
                }
            } break;
            default:
                break;
            }
        } else if(mods & KMOD_ALT_FLAG) {
            switch(key) {
            case KEY_LEFT_ALT:
            case KEY_RIGHT_ALT:
                measure_mode = true;
                set_cursor(crosshair_cursor);
                if(selected_layer != nullptr) {
                    selected_layer->drawer->clear_entity_flags(entity_flags_t::hovered);
                }
                set_mouse_mode(mouse_drag_none);
                break;
            default:
                break;
            }
        }
    } else if(action == ACTION_RELEASE) {
        switch(key) {
        case KEY_LEFT_ALT:
        case KEY_RIGHT_ALT:
            measure_mode = false;
            set_cursor(nullptr);
            break;
        default:
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_scroll(double xoffset, double yoffset)
{
    set_active();
    double scale_factor = (yoffset > 0) ? 0.9 : 1.1;
    zoom_at_point(world_pos_from_viewport_pos(mouse_pos), scale_factor);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_button(int button, int action, int mods)
{
    set_active();

    switch(action) {
    case ACTION_PRESS:
        switch(button) {
        case MOUSE_BUTTON_LEFT:
            if(measure_mode) {
                // Start measuring (orthogonal to mouse_mode)
                measure_start_world = world_pos_from_viewport_pos(mouse_pos);
                measure_end_world = measure_start_world;
                measure_dragging = true;
                measure_line_visible = false;
            } else if((mods & KMOD_CTRL_FLAG) != 0) {
                set_mouse_mode(mouse_drag_zoom_select);
            } else {
                set_mouse_mode(mouse_drag_maybe_select);
            }
            break;
        case MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_pan);
            break;
        case MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_zoom);
            break;
        default:
            break;
        }
        break;
    case ACTION_RELEASE:
        switch(button) {
        case MOUSE_BUTTON_LEFT:
            if(measure_dragging) {
                // Finalize measurement
                measure_end_world = world_pos_from_viewport_pos(mouse_pos);
                measure_line_visible = true;
                measure_dragging = false;
            } else if(mouse_mode == mouse_drag_zoom_select) {
                rect drag_rect_corrected = correct_aspect_ratio(viewport_rect.aspect_ratio(), drag_rect, aspect_expand);
                vec2d mn = drag_rect_corrected.min_pos;
                vec2d mx = drag_rect_corrected.max_pos;
                rect d = rect{ mn, mx }.normalize();
                if(d.width() > 2 && d.height() > 2) {
                    zoom_to_rect(world_rect_from_viewport_rect({ mn, mx }));
                    should_fit_to_viewport = false;
                }
                set_mouse_mode(mouse_drag_none);
            } else if(mouse_mode == mouse_drag_select && selected_layer != nullptr) {
                selected_layer->drawer->select_hovered_entities();
                set_mouse_mode(mouse_drag_none);
            } else {
                set_mouse_mode(mouse_drag_none);
            }
            break;
        case MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_none);
            break;
        case MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_none);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_move(double xpos, double ypos)
{
    // Wrap cursor to opposite edge when panning for infinite pan
    if(mouse_mode == mouse_drag_pan) {
        bool warped = false;
        double new_x = xpos;
        double new_y = ypos;

        if(xpos <= 0) {
            new_x = window_width - 2;
            warped = true;
        } else if(xpos >= window_width - 1) {
            new_x = 1;
            warped = true;
        }

        if(ypos <= 0) {
            new_y = window_height - 2;
            warped = true;
        } else if(ypos >= window_height - 1) {
            new_y = 1;
            warped = true;
        }

        if(warped) {
            set_cursor_pos(new_x, new_y);
            mouse_pos = { new_x - viewport_xpos, viewport_height - (new_y - viewport_ypos) };
            drag_mouse_start_pos = mouse_pos;
            prev_mouse_pos = mouse_pos;
            set_active();
            return;
        }
    }

    mouse_pos = { xpos - viewport_xpos, viewport_height - (ypos - viewport_ypos) };
    set_active();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::handle_mouse()
{
    if(!prev_mouse_pos.equal(mouse_pos)) {

        world_mouse_pos = world_pos_from_viewport_pos(mouse_pos);

        prev_mouse_pos = mouse_pos;

        switch(mouse_mode) {

        case mouse_drag_pan: {
            vec2d new_mouse_pos = world_pos_from_viewport_pos(mouse_pos);
            vec2d old_mouse_pos = world_pos_from_viewport_pos(drag_mouse_start_pos);
            vec2d diff = new_mouse_pos.subtract(old_mouse_pos).negate();
            view_rect = view_rect.offset(diff);
            drag_mouse_start_pos = mouse_pos;
            should_fit_to_viewport = false;
        } break;

        case mouse_drag_zoom: {
            if(ignore_mouse_moves <= 0) {
                vec2d d = mouse_pos.subtract(drag_mouse_cur_pos);
                double factor = d.y * 0.01;
                factor = std::max(-0.25, std::min(factor, 0.25));
                zoom_at_point(mouse_world_pos, 1.0 - factor);
                should_fit_to_viewport = false;
            } else {
                ignore_mouse_moves -= 1;
            }
            drag_mouse_cur_pos = mouse_pos;
        } break;

        case mouse_drag_zoom_select: {
            drag_mouse_cur_pos = mouse_pos;
            if(drag_mouse_cur_pos.subtract(drag_mouse_start_pos).length() > 4) {
                drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos }.normalize();
            }
        } break;

        case mouse_drag_maybe_select: {
            if(mouse_pos.subtract(drag_mouse_start_pos).length() > drag_select_offset_start_distance) {
                set_active_entity(nullptr);
                set_mouse_mode(mouse_drag_select);
                mouse_world_pos = world_pos_from_viewport_pos(mouse_pos);
            }
        } break;

        case mouse_drag_select: {
            drag_mouse_cur_pos = mouse_pos;
            drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
            if(selected_layer != nullptr) {
                if(drag_rect.min_pos.x > drag_rect.max_pos.x) {
                    selected_layer->drawer->flag_touching_entities(
                        board_rect_from_viewport_rect(drag_rect), entity_flags_t::hovered | entity_flags_t::selected, entity_flags_t::hovered);
                } else {
                    selected_layer->drawer->flag_enclosed_entities(
                        board_rect_from_viewport_rect(drag_rect), entity_flags_t::hovered | entity_flags_t::selected, entity_flags_t::hovered);
                }
            }
        } break;

        default:
        case mouse_drag_none: {
            // Update measurement end point while dragging (not during zoom/pan)
            if(measure_dragging) {
                measure_end_world = world_pos_from_viewport_pos(mouse_pos);
            }
            // Just hovering, highlight entities under the mouse if selected_layer != nullptr
            if(selected_layer != nullptr && !measure_mode) {
                vec2d pos = board_pos_from_viewport_pos(mouse_pos);
                selected_layer->drawer->flag_entities_at_point(pos, entity_flags_t::hovered, entity_flags_t::hovered);
            }
        } break;
        }
    }
}


//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_closed()
{
    bool should_save_files = pool.get_active_job_count(job_type_load_gerber) == 0;
    save_settings(config_path(app_name, settings_filename), should_save_files);
    pool.shut_down();
    // after shut_down, all jobs are done, safe to delete deferred layers
    for(auto *l : layers) {
        delete l;
    }
    layers.clear();
    NFD_Quit();
    if(crosshair_cursor != nullptr) {
        SDL_DestroyCursor(crosshair_cursor);
    }
    gpu_window::on_closed();
    for(auto *l : layers) {
        l->gpu_resources.release(gpu_dev);
    }
    gpu_dev.release_buffer(gpu_quad_vbo);
    gpu_dev.release_buffer(gpu_overlay_vbo);
    gpu_render_target.cleanup(gpu_dev);
    gpu_pipelines.cleanup(gpu_dev);
    gpu_dev.shutdown();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_settings(std::filesystem::path const &path)
{
    if(settings.load(path)) {
        LOG_DEBUG("Settings loaded...");
        window_state.width = settings.window_width;
        window_state.height = settings.window_height;
        window_state.x = settings.window_xpos;
        window_state.y = settings.window_ypos;
        window_state.isMaximized = settings.window_maximized;
        for(auto const &layer : settings.files) {
            add_gerber(layer);
        }
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_window_size(int w, int h)
{
    gpu_window::on_window_size(w, h);
    window_width = w;
    window_height = h;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_window_refresh()
{
    gpu_window::on_window_refresh();
    if(frames == 0) {
        return;    // needs a proper first frame before refresh renders
    }
    on_frame();
    if(should_fit_to_viewport) {
        view_rect = target_view_rect;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::close_all_layers()
{
    LOG_INFO("CLOSE ALL");
    set_active_entity(nullptr);
    select_layer(nullptr);
    layers.remove_if([](gerber_layer *l) {
        if(l->job_count.load() == 0) {
            delete l;
            return true;
        }
        l->marked_for_deletion = true;
        return false;
    });
}

//////////////////////////////////////////////////////////////////////

gerber_layer *gerber_explorer::get_outline_layer() const
{
    for(auto *l : layers) {
        if(l->is_outline_layer) {
            return l;
        }
    }
    return nullptr;
}

//////////////////////////////////////////////////////////////////////

bool gerber_explorer::layer_is_visible(gerber_layer const *layer) const
{
    if(layer->marked_for_deletion) {
        return false;
    }
    if(selected_layer != nullptr && isolate_selected_layer) {
        return layer == selected_layer;
    }
    if(!layer->visible) {
        return false;
    }
    if(settings.board_view == board_view_bottom && !is_bottom_layer(layer->layer_order)) {
        return false;
    }
    if(settings.board_view == board_view_top && !is_top_layer(layer->layer_order)) {
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::save_settings(std::filesystem::path const &path, bool save_files)
{
    LOG_INFO("save settings");
    window_state = get_window_state();
    settings.window_width = window_state.width;
    settings.window_height = window_state.height;
    settings.window_xpos = window_state.x;
    settings.window_ypos = window_state.y;
    settings.window_maximized = window_state.isMaximized;

    if(save_files) {
        LOG_INFO("saving files:");
        settings.files.clear();
        int index = 1;
        for(auto it = layers.crbegin(); it != layers.crend(); ++it) {
            gerber_layer *layer = *it;
            if(layer->is_valid()) {
                settings.files.emplace_back(layer->filename(), gpu::color_to_string(layer->fill_color), layer->visible, layer->invert, index);
                LOG_INFO("> {}", layer->filename());
                index += 1;
            }
        }
    }
    settings.save(path);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::set_mouse_mode(mouse_drag_action action)
{
    switch(action) {

    case mouse_drag_none:
        zoom_anim = mouse_mode == mouse_drag_zoom_select;
        set_input_mode_cursor_normal();
        set_cursor(measure_mode ? crosshair_cursor : nullptr);
        break;

    case mouse_drag_pan:
        drag_mouse_start_pos = mouse_pos;
        zoom_anim = false;
        break;

    case mouse_drag_zoom:
        zoom_anim = false;
        set_input_mode_cursor_disabled();
        mouse_world_pos = world_pos_from_viewport_pos(mouse_pos);
        drag_mouse_cur_pos = mouse_pos;
        drag_mouse_start_pos = mouse_pos;
        // ignore the next 2 mouse moves because sometimes they are kind of
        // random, something to do with cursor mode switching
        ignore_mouse_moves = 2;
        break;

    case mouse_drag_zoom_select:
        zoom_anim = false;
        drag_mouse_start_pos = mouse_pos;
        drag_rect = {};
        break;

    case mouse_drag_maybe_select: {
        zoom_anim = false;
        drag_mouse_start_pos = mouse_pos;
        if(selected_layer != nullptr) {
            std::vector<int> entity_indices;
            mouse_world_pos = board_pos_from_viewport_pos(mouse_pos);
            selected_layer->drawer->find_entities_at_point(mouse_world_pos, entity_indices);
            if(entity_indices != active_entities) {
                active_entity_index = 0;
            }
            active_entities = entity_indices;
            active_entity = nullptr;
            selected_layer->drawer->clear_entity_flags(entity_flags_t::all_select);
            if(!active_entities.empty()) {
                if(active_entity_index < (int)active_entities.size()) {
                    tesselator_entity &e = selected_layer->drawer->entities[active_entities[active_entity_index]];
                    set_active_entity(&e);
                } else {
                    active_entity = nullptr;
                }
                active_entity_index = (active_entity_index + 1) % (active_entities.size() + 1);
                for(int i : entity_indices) {
                    selected_layer->drawer->entities[i].flags |= entity_flags_t::hovered;
                }
            }
        }
    } break;

    case mouse_drag_select:
        zoom_anim = false;
        drag_mouse_cur_pos = mouse_pos;
        drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
        break;
    }

    mouse_mode = action;
}

//////////////////////////////////////////////////////////////////////

bool gerber_explorer::on_init()
{

#ifdef WIN32
    HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
    if(hIcon) {
        HWND hwnd = (HWND)get_native_window_handle();
        if(hwnd) {
            SendMessage(hwnd, WM_SETICON, ICON_SMALL2, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }
    }
#endif

    pool.start_workers();

    NFD_Init();

    get_window_size(&window_width, &window_height);
    window_size.x = window_width;
    window_size.y = window_height;
    viewport_rect = { { 0, 0 }, { (double)window_width, (double)window_height } };
    view_rect = viewport_rect;
    viewport_size = viewport_rect.size();

    if(!gpu_dev.init(window)) {
        LOG_ERROR("GPU device init failed");
        return false;
    }

    SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(gpu_dev.gpu, window);

    if(!gpu_pipelines.init(gpu_dev, swapchain_format, SDL_GPU_SAMPLECOUNT_1)) {
        LOG_ERROR("GPU pipeline init failed");
        gpu_dev.shutdown();
        return false;
    }

    // Initialize ImGui backends
    ImGui_ImplSDL3_InitForOther(window);

    ImGui_ImplSDLGPU3_InitInfo imgui_gpu_info{};
    imgui_gpu_info.Device = gpu_dev.gpu;
    imgui_gpu_info.ColorTargetFormat = swapchain_format;
    imgui_gpu_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&imgui_gpu_info);

    crosshair_cursor = create_system_cursor(SDL_SYSTEM_CURSOR_CROSSHAIR);

    {
        // Query max supported MSAA for our RT format
        max_multisamples = 1;
        SDL_GPUSampleCount test_counts[] = { SDL_GPU_SAMPLECOUNT_2, SDL_GPU_SAMPLECOUNT_4, SDL_GPU_SAMPLECOUNT_8 };
        int test_values[] = { 2, 4, 8 };
        for(int i = 0; i < 3; ++i) {
            if(SDL_GPUTextureSupportsSampleCount(gpu_dev.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, test_counts[i])) {
                max_multisamples = test_values[i];
            }
        }
    }

    LOG_INFO("MAX Multisamples: {}", max_multisamples);

    load_settings(config_path(app_name, settings_filename));

    if(settings.multisamples > max_multisamples) {
        settings.multisamples = max_multisamples;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::file_open()
{
    const nfdpathset_t *paths;
    nfdopendialogu8args_t args{};
    nfdresult_t result = NFD_OpenDialogMultipleU8_With(&paths, &args);
    switch(result) {
    case NFD_OKAY: {
        nfdpathsetsize_t path_count;
        if(NFD_PathSet_GetCount(paths, &path_count) == NFD_OKAY) {
            for(nfdpathsetsize_t i = 0; i < path_count; ++i) {
                nfdu8char_t *outPath;
                if(NFD_PathSet_GetPath(paths, i, &outPath) == NFD_OKAY) {
                    next_layer_color += 1;
                    next_layer_color %= std::size(layer_colors);
                    // index -1 means try to determine from layer classification
                    add_gerber(settings::layer_t{ outPath, gpu::colorf4(layer_colors[next_layer_color]).to_string(), true, false, -1 });
                    // NFD_FreePathU8(outPath);
                }
            }
        }
        NFD_PathSet_Free(paths);
    } break;
    case NFD_CANCEL:
        LOG_DEBUG("Cancelled");
        break;
    default:
        LOG_ERROR("Error: {}", NFD_GetError());
        break;
    }
}

//////////////////////////////////////////////////////////////////////

std::expected<std::filesystem::path, std::error_code> gerber_explorer::save_file_dialog(char const *filename)
{
    nfdu8char_t *path;
    nfdresult_t result = NFD_SaveDialogU8(&path, nullptr, 0, nullptr, filename);
    switch(result) {
    case NFD_OKAY:
        return { path };
    case NFD_CANCEL:
        LOG_DEBUG("Cancelled");
        return std::unexpected(std::make_error_code(std::errc::operation_canceled));
    default:
        LOG_ERROR("Error: {}", NFD_GetError());
        break;
    }
    return std::unexpected(std::make_error_code(std::errc::io_error));
}

//////////////////////////////////////////////////////////////////////

std::expected<std::filesystem::path, std::error_code> gerber_explorer::load_file_dialog()
{
    nfdu8char_t *path;
    nfdresult_t result = NFD_OpenDialogU8(&path, nullptr, 0, nullptr);
    switch(result) {
    case NFD_OKAY:
        return { path };
    case NFD_CANCEL:
        LOG_DEBUG("Cancelled");
        return std::unexpected(std::make_error_code(std::errc::operation_canceled));
    default:
        LOG_ERROR("Error: {}", NFD_GetError());
        break;
    }
    return std::unexpected(std::make_error_code(std::errc::io_error));
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::select_layer(gerber_layer *layer)
{
    if(selected_layer != nullptr) {
        selected_layer->drawer->clear_entity_flags(entity_flags_t::all_select);
    }
    if(layer != selected_layer) {
        active_entity = nullptr;
    }
    selected_layer = layer;
    if(selected_layer == nullptr) {
        isolate_selected_layer = false;
    }
}

//////////////////////////////////////////////////////////////////////
// It's annoying that we retesselate the layer when all we want is
// to generate the mask...

void gerber_explorer::tesselate_layer(gerber_layer *layer, tesselation_options_t options, double pixels_per_world_unit)
{
    pool.add_job(job_type_tesselate, [this, layer, options, pixels_per_world_unit](std::stop_token st) {
        layer->job_count.fetch_add(1);
        DEFER(layer->job_count.fetch_sub(1));
        bool force_outline = (options & tesselation_options_force_outline) != 0;

        // Atomically claim the idle drawer. If another retesselation job is already
        // running for this layer (possible in the non-blocking dynamic-tess path),
        // bail out - the running job will produce a result and another retesselation
        // will be triggered by the next debounce if needed.
        int d;
        {
            std::lock_guard l(layer_drawer_mutex);
            if(layer->retesselating) {
                return;
            }
            d = 1 - layer->current_drawer;
            layer->retesselating = true;
        }

        gerber_drawer *other_drawer = &layer->drawers[d];
        using namespace gerber_lib;
        auto layer_type = layer->layer_type();
        layer->is_outline_layer = force_outline || is_layer_type(layer_type, layer::type_t::board) || is_layer_type(layer_type, layer::type_t::outline);
        other_drawer->pixels_per_world_unit = layer->is_outline_layer ? 0 : pixels_per_world_unit;
        other_drawer->tesselation_quality = layer->is_outline_layer ? tesselation_quality::high : settings.tesselation_quality;
        other_drawer->set_gerber(&layer->file);
        if(layer->is_outline_layer) {
            other_drawer->create_mask();
        }
        // transfer entity flags (hovered/selected/active) from old drawer to new
        // NOTE: only transfer selection flags - fill/clear come from the new tesselation
        // and must not be overwritten (old_drawer->entity_flags is zero for layers that
        // were never rendered, which would wipe out the fill/clear flags)
        gerber_drawer *old_drawer = &layer->drawers[layer->current_drawer];
        for(auto &e : other_drawer->entities) {
            int id = e.entity_id();
            if(id >= 0 && id < (int)old_drawer->entity_flags.size()) {
                e.flags = (e.flags & ~entity_flags_t::all_select) | (old_drawer->entity_flags[id] & entity_flags_t::all_select);
            }
        }
        {
            std::lock_guard l(layer_drawer_mutex);
            layer->current_drawer = d;
            layer->retesselating = false;
        }
    });
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_gerber(settings::layer_t const &layer_to_load)
{
    gerber_layer *layer = new gerber_layer();
    layer->init();
    gerber_lib::gerber_error_code err = layer->file.parse_file(layer_to_load.filename.c_str());
    if(err != gerber_lib::ok) {
        LOG_ERROR("Error loading {} ({})", layer_to_load.filename, gerber_lib::get_error_text(err));
        delete layer;
        return;
    }

    gerber_lib::gerber_file &g = layer->file;
    layer->index = layer_to_load.index;
    layer->name = std::format("{}", std::filesystem::path(g.filename).filename().string());
    layer->visible = layer_to_load.visible;
    layer->clear_color = gpu::colors::black;
    layer->drawer = &layer->drawers[0];

    layer_defaults_t d = get_defaults_for_layer_type(g.layer_type);
    if(layer->index == -1) {
        layer->index = g.layer_type;
        LOG_DEBUG("{}:{} ({})", layer->name, g.image.info.polarity, d.is_inverted);
        if(g.image.info.polarity == gerber_lib::polarity_unspecified) {
            layer->invert = d.is_inverted;
        } else {
            layer->invert = g.image.info.polarity == gerber_lib::polarity_negative;
        }
        layer->fill_color = d.color;
    } else {
        layer->invert = layer_to_load.inverted;
        layer->fill_color = gpu::color_from_string(layer_to_load.color);
    }
    next_index = std::max(layer->index + 1, next_index);
    layer->layer_order = d.layer_order;

    LOG_DEBUG("Finished loading {}, \"{}\"", layer->index, layer_to_load.filename);

    pool.add_job(job_type_tesselate, [layer, this]([[maybe_unused]] std::stop_token st) {
        layer->job_count.fetch_add(1);
        DEFER(layer->job_count.fetch_sub(1));

        using namespace gerber_lib;
        auto layer_type = layer->layer_type();
        layer->is_outline_layer = is_layer_type(layer_type, layer::type_t::board) || is_layer_type(layer_type, layer::type_t::outline);
        layer->drawer->tesselation_quality = layer->is_outline_layer ? tesselation_quality::high : settings.tesselation_quality;
        layer->drawer->set_gerber(&layer->file);
        LOG_DEBUG("Tesselated ({}:{}) {}", layer_type_name(layer_type), layer->is_outline_layer, layer->filename());
        if(layer->is_outline_layer) {
            layer->drawer->create_mask();
        }

        // inform main thread that there's a new layer available and wait for it to pick it up
        {
            std::lock_guard loaded_lock(loaded_mutex);
            loaded_layers.push_back(layer);
        }
        bool loaded = false;
        while(!loaded && !st.stop_requested()) {
            {
                std::lock_guard loaded_lock(loaded_mutex);
                loaded = loaded_layers.empty();
                SDL_Event e{}; e.type = SDL_EVENT_USER; SDL_PushEvent(&e);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    });
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::add_gerber(settings::layer_t const &layer)
{
    settings::layer_t l = layer;
    pool.add_job(job_type_load_gerber, [l, this]([[maybe_unused]] std::stop_token st) { load_gerber(l); });
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::set_active_entity(tesselator_entity *entity)
{
    if(active_entity != nullptr) {
        active_entity->flags &= ~entity_flags_t::active;
    }
    active_entity = entity;
    if(active_entity == nullptr) {
        active_entity_info.clear();
        return;
    }
    active_entity->flags |= entity_flags_t::active;
    using namespace gerber_lib;

    auto &info = active_entity_info;
    info.clear();

    gerber_net *net = active_entity->net;
    gerber_level *level = net->level;
    gerber_aperture *aperture = nullptr;

    double scale = 1.0;
    char const *units = "mm";
    if(settings.units == settings::units_inch) {
        scale = 1.0 / 25.4;
        units = "in";
    }

    auto coord = [&](double v) { return v * scale; };

    // Look up aperture
    if(net->aperture != 0 && selected_layer != nullptr) {
        auto it = selected_layer->file.image.apertures.find(net->aperture);
        if(it != selected_layer->file.image.apertures.end()) {
            aperture = it->second;
        }
    }

    // Find source line number from the entity list
    int entity_index = active_entity->entity_id();
    int source_line = 0;
    if(selected_layer != nullptr && entity_index >= 0 && entity_index < (int)selected_layer->file.entities.size()) {
        source_line = selected_layer->file.entities[entity_index].line_number_begin;
    }

    // Entity ID + source line
    if(source_line > 0) {
        info.push_back(std::format("Entity: {} (line {})", net->entity_id, source_line));
    } else {
        info.push_back(std::format("Entity: {}", net->entity_id));
    }

    // Type
    {
        std::string type_str;
        switch(net->aperture_state) {
        case aperture_state_flash:
            type_str = "Flash";
            break;
        case aperture_state_on:
            switch(net->interpolation_method) {
            case interpolation_linear: type_str = "Linear"; break;
            case interpolation_clockwise_circular: type_str = "Arc CW"; break;
            case interpolation_counterclockwise_circular: type_str = "Arc CCW"; break;
            case interpolation_region_start:
                type_str = std::format("Region ({} points, {} contours)", net->num_region_points, active_entity->num_contours);
                break;
            default: type_str = "Draw"; break;
            }
            break;
        case aperture_state_off: type_str = "Move"; break;
        }
        info.push_back(std::format("Type: {}", type_str));
    }

    // Aperture
    if(aperture != nullptr) {
        if(aperture->aperture_type >= aperture_type_macro && aperture->aperture_macro != nullptr) {
            info.push_back(std::format("Aperture: D{} Macro \"{}\"", net->aperture, aperture->aperture_macro->name));
        } else {
            std::string ap_desc = aperture->get_description(1.0 / scale, units);
            info.push_back(std::format("Aperture: D{} {}", net->aperture, ap_desc));
        }
    }

    // Position/geometry details
    switch(net->aperture_state) {
    case aperture_state_flash:
        info.push_back(std::format("Position: ({:.4f}, {:.4f}) {}", coord(net->end.x), coord(net->end.y), units));
        break;

    case aperture_state_on:
        if(net->interpolation_method == interpolation_linear) {
            double dx = net->end.x - net->start.x;
            double dy = net->end.y - net->start.y;
            double len = std::sqrt(dx * dx + dy * dy);
            info.push_back(std::format("From: ({:.4f}, {:.4f}) {}", coord(net->start.x), coord(net->start.y), units));
            info.push_back(std::format("To: ({:.4f}, {:.4f}) {}", coord(net->end.x), coord(net->end.y), units));
            info.push_back(std::format("Length: {:.4f} {}", coord(len), units));
        } else if(net->interpolation_method == interpolation_clockwise_circular ||
                  net->interpolation_method == interpolation_counterclockwise_circular) {
            auto const &arc = net->circle_segment;
            double radius = arc.size.x / 2.0;
            info.push_back(std::format("Center: ({:.4f}, {:.4f}) {}", coord(arc.pos.x), coord(arc.pos.y), units));
            info.push_back(std::format("Radius: {:.4f} {}", coord(radius), units));
            info.push_back(std::format("Angles: {:.1f}\xc2\xb0 \xe2\x86\x92 {:.1f}\xc2\xb0 (sweep {:.1f}\xc2\xb0)",
                                       arc.start_angle, arc.end_angle, arc.sweep_angle()));
        }
        break;

    default:
        break;
    }

    // Polarity
    char const *polarity_str = "Dark";
    if(level->polarity == polarity_clear || level->polarity == polarity_negative) {
        polarity_str = "Clear";
    }
    info.push_back(std::format("Polarity: {}", polarity_str));

    // Bounding box
    rect const &bb = active_entity->bounds;
    if(!bb.is_empty_rect()) {
        info.push_back(std::format("Bounds: {:.4f} x {:.4f} {}", coord(bb.width()), coord(bb.height()), units));
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::export_stl(std::filesystem::path filepath, gerber_layer *l, rect board_ext)
{
    gerber_layer *outline_layer{ nullptr };
    if(l->invert) {
        for(auto const layer : layers) {
            if(outline_layer == nullptr && layer->is_outline_layer && layer->got_mask) {
                outline_layer = layer;
                break;
            }
        }
    }
    pool.add_job(job_type_export, [this, filepath, l, outline_layer, board_ext] (std::stop_token st) {
        l->job_count.fetch_add(1);
        if(outline_layer != nullptr) {
            outline_layer->job_count.fetch_add(1);
        }
        LOG_CONTEXT("export", debug);
        LOG_INFO("Export {} as {}", l->name, filepath.string());
        gerber_3d::gpu_3d_drawer drawer;
        drawer.init();
        drawer.tesselation_quality = settings.tesselation_quality;
        drawer.set_gerber(&l->file);
        if(l->invert) {
            using namespace Clipper2Lib;
            Paths64 openings = PolyTreeToPaths64(drawer.resolved_tree);
            Paths64 outline;
            if(outline_layer != nullptr) {
                // resolve outline layer with fine arc tessellation to get clean board shape
                gerber_3d::gpu_3d_drawer outline_drawer;
                outline_drawer.init();
                outline_drawer.tesselation_quality = settings.tesselation_quality;
                outline_drawer.set_gerber(&outline_layer->file);
                // take only outer contours (top-level children), not holes
                for(auto const &child : outline_drawer.resolved_tree) {
                    outline.push_back(child->Polygon());
                }
                outline_drawer.release();
            } else {
                int64_t S = gerber_3d::gpu_3d_drawer::CLIPPER_SCALE;
                outline.push_back({
                    {static_cast<int64_t>(board_ext.min_pos.x * S), static_cast<int64_t>(board_ext.min_pos.y * S)},
                    {static_cast<int64_t>(board_ext.max_pos.x * S), static_cast<int64_t>(board_ext.min_pos.y * S)},
                    {static_cast<int64_t>(board_ext.max_pos.x * S), static_cast<int64_t>(board_ext.max_pos.y * S)},
                    {static_cast<int64_t>(board_ext.min_pos.x * S), static_cast<int64_t>(board_ext.max_pos.y * S)}
                });
            }
            Clipper64 clipper;
            clipper.AddSubject(outline);
            clipper.AddClip(openings);
            drawer.resolved_tree.Clear();
            clipper.Execute(ClipType::Difference, FillRule::NonZero, drawer.resolved_tree);
        }
        drawer.extrude(0.035);
        drawer.export_stl(filepath.string());
        drawer.release();
        LOG_INFO("Completed export to {}", filepath.string());
        if(outline_layer != nullptr) {
            outline_layer->job_count.fetch_sub(1);
        }
        l->job_count.fetch_sub(1);
    });
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::ui()
{
    auto is_active = [] {
        ImGuiIO &io = ImGui::GetIO();
        if(io.Ctx->DimBgRatio != 0.0f && io.Ctx->DimBgRatio != 1.0f) {
            return true;
        }
        if(io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
            return true;
        }
        for(int i = 0; i < 5; ++i) {
            if(io.MouseClicked[i] || io.MouseReleased[i]) {
                return true;
            }
        }
        return false;
    };

    if(is_active()) {
        set_active();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Open Layers", "Ctrl-O", nullptr)) {
                file_open();
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Save Settings", "Ctrl-S", nullptr)) {
                auto save_path = save_file_dialog("settings.json");
                if(save_path.has_value()) {
                    save_settings(save_path.value(), true);
                }
            }
            if(ImGui::MenuItem("Load Settings", "Ctrl-L", nullptr)) {
                auto load_path = load_file_dialog();
                if(load_path.has_value()) {
                    load_settings(load_path.value());
                }
            }
            // ImGui::MenuItem("Stats", nullptr, &show_stats);
            // ImGui::MenuItem("Options", nullptr, &show_options);
            ImGui::Separator();
            if(ImGui::MenuItem("Close all", nullptr, nullptr)) {
                close_all_layers();
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Exit", "Esc", nullptr)) {
                set_should_close();
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View")) {
            if(ImGui::MenuItem("Fit to window", "F", nullptr, !layers.empty())) {
                fit_to_viewport();
            }
            ImGui::MenuItem("Flip X", "X", &settings.flip_x);
            ImGui::MenuItem("Flip Y", "Y", &settings.flip_y);
            ImGui::MenuItem("Toolbar", "", &settings.view_toolbar);

            // Background color
            ImVec2 pos = ImGui::GetCursorScreenPos();
            if(ImGui::Selectable("Background", false, ImGuiSelectableFlags_DontClosePopups)) {
                ImGui::OpenPopup("BackgroundColorPickerPopup");
            }
            float boxSize = ImGui::GetFontSize();
            float posX = pos.x + ImGui::GetItemRectSize().x - boxSize - ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorScreenPos(ImVec2(posX, pos.y));
            ImGui::ColorButton("##bgcolor_btn",
                               { settings.background_color.r, settings.background_color.g, settings.background_color.b, 1.0f },
                               ImGuiColorEditFlags_NoAlpha,
                               ImVec2(boxSize, boxSize));
            if(ImGui::BeginPopup("BackgroundColorPickerPopup")) {
                ImGui::ColorPicker3("##bgcolor_picker", (float *)settings.background_color, ImGuiColorEditFlags_NoAlpha);
                ImGui::EndPopup();
            }

            ImGui::MenuItem("Wireframe", "W", &settings.wireframe);
            ImGui::MenuItem("Show Axes", "A", &settings.show_axes);
            ImGui::MenuItem("Show Extent", "E", &settings.show_extent);
            if(ImGui::BeginMenu("Units")) {
                if(ImGui::MenuItem("MM", "", settings.units == settings::units_mm)) {
                    settings.units = settings::units_mm;
                    set_active_entity(active_entity);
                }
                if(ImGui::MenuItem("Inch", "", settings.units == settings::units_inch)) {
                    settings.units = settings::units_inch;
                    set_active_entity(active_entity);
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Anti-Aliasing")) {
                int msaa_options[] = { 1, 2, 4, 8 };
                int num_options = 0;
                for(int i = 0; i < 4; ++i) {
                    if(msaa_options[i] <= max_multisamples) {
                        num_options = i + 1;
                    }
                }
                for(int i = 0; i < num_options; ++i) {
                    char label[16];
                    snprintf(label, sizeof(label), "%dx", msaa_options[i]);
                    if(ImGui::MenuItem(label, "", settings.multisamples == msaa_options[i])) {
                        settings.multisamples = msaa_options[i];
                    }
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Tesselation")) {
                if(ImGui::Checkbox("Dynamic", &settings.dynamic_tesselation)) {
                    if(settings.dynamic_tesselation) {
                        last_tess_ppwu = 0;
                    }
                    retesselate = true;
                }
                if(ImGui::SliderInt("Quality",
                                    &settings.tesselation_quality,
                                    tesselation_quality::low,
                                    tesselation_quality::high,
                                    tesselation_quality_name(settings.tesselation_quality))) {
                    retesselate = true;
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Outline")) {
                ImGui::SliderFloat("##val", &settings.outline_width, 0.0f, 8.0f, "%.1f");
                ImGui::ColorEdit4("Outline color",
                                  (float *)&settings.outline_color,
                                  ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

#if defined(_DEBUG)
        static int frames = 0;
        frames += 1;
        std::string text = std::format("Frame {:07d} {:06.2f}ms {:06.2f}ms", frames, last_frame_elapsed_time * 1000.0, last_frame_cpu_time * 1000.0);
        float text_width = ImGui::CalcTextSize(text.c_str()).x;
        float posX = ImGui::GetWindowWidth() - text_width - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(posX);
        ImGui::Text("%s", text.c_str());
#endif
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar(1);

    if(settings.view_toolbar) {
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDecoration);
        {
            if(ImGui::Button("Open " MATSYM_file_open)) {
                file_open();
            }
            ImGui::SameLine();
            if(ImGui::Button("Close all " MATSYM_cancel_presentation)) {
                ImGui::OpenPopup("Close All");
            }
            ImGui::SameLine();
            if(ImGui::Button("Fit " MATSYM_fit_screen)) {
                fit_to_viewport();
            }
            ImGui::SameLine();
            if(ImGui::Button("X " MATSYM_swap_horiz)) {
                settings.flip_x = !settings.flip_x;
            }
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::SetItemTooltip("Flip X");
            }
            ImGui::SameLine();
            if(ImGui::Button("Y " MATSYM_swap_vert)) {
                settings.flip_y = !settings.flip_y;
            }
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::SetItemTooltip("Flip Y");
            }
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("View");
            ImGui::SameLine();
            float max_width = 0.0f;
            for(int n = 0; n < board_view_num_views; ++n) {
                max_width = std::max(ImGui::CalcTextSize(board_view_names[n]).x, max_width);
            }
            float combo_width = max_width + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(combo_width);
            if(ImGui::BeginCombo("##view_combo", board_view_names[settings.board_view], ImGuiComboFlags_PopupAlignLeft)) {
                for(int n = 0; n < board_view_num_views; ++n) {
                    bool is_selected = settings.board_view == n;
                    if(ImGui::Selectable(board_view_names[n], is_selected)) {
                        settings.board_view = n;
                        isolate_selected_layer = false;
                    }
                    if(is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            if(MsgBox("Close All", "Are you sure you want to close all the layers?") == 1) {
                close_all_layers();
            }
        }
        ImGui::End();
    }

    gerber_layer *item_to_move = nullptr;
    gerber_layer *item_target = nullptr;
    gerber_layer *item_to_delete = nullptr;
    bool move_before = false;

    ImGui::Begin("Files");
    {
        bool any_item_hovered = false;
        int constexpr num_controls = 3;
        float controls_width = ImGui::GetFrameHeight() * num_controls + ImGui::GetStyle().ItemSpacing.x * num_controls;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));

        if(ImGui::BeginTable("Layers", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, controls_width);

            for(auto it = layers.rbegin(); it != layers.rend(); ++it) {
                gerber_layer *l = *it;
                if(l->marked_for_deletion) {
                    continue;
                }
                ImGui::PushID(l);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImVec2 row_min = ImGui::GetCursorScreenPos();    // Capture start of row
                float row_height = ImGui::GetFrameHeight();
                ImVec2 row_size = ImVec2(ImGui::GetContentRegionAvail().x + controls_width, row_height);
                ImVec2 row_max = ImVec2(row_min.x + row_size.x, row_min.y + row_size.y);
                float mid_y = row_min.y + (row_height / 2.0f);
                bool is_selected = (selected_layer == l);

                auto flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;

                ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                if(!layer_is_visible(*it)) {
                    color.w *= 0.5f;
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);

                std::string text = std::format("{} {}", l->name, l->is_outline_layer ? MATSYM_check_box_outline_blank : "");

                if(ImGui::Selectable(text.c_str(), is_selected, flags, ImVec2(0, row_height))) {
                    select_layer(l);
                    // what to do if they select a hidden layer and isolation is on?
                    // if layer is visible, just make it active and keep isolation
                    // if layer is invisible...?
                }
                ImGui::PopStyleColor();

                if(ImGui::BeginPopupContextItem("##popup")) {
                    select_layer(l);
                    ImGui::MenuItem(isolate_selected_layer ? "UnIsolate" : "Isolate", nullptr, &isolate_selected_layer);
                    ImGui::MenuItem("Invert", nullptr, &l->invert);
                    bool is_outline = l->is_outline_layer;
                    if(ImGui::MenuItem("Outline", nullptr, &is_outline)) {
                        if(is_outline) {
                            set_outline_layer(l);
                        } else {
                            set_outline_layer(nullptr);
                        }
                    }
                    if(ImGui::MenuItem("Export STL")) {
                        std::filesystem::path p(l->name);
                        p.replace_extension(".stl");
                        auto save_path = save_file_dialog(p.string().c_str());
                        if(save_path.has_value()) {
                            export_stl(save_path.value(), l, board_extent);
                        }
                    }
                    ImGui::EndPopup();
                }

                if(ImGui::IsItemHovered()) {
                    any_item_hovered = true;
                }

                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
                    ImGui::SetDragDropPayload("GERBER_LAYER", &l, sizeof(gerber_layer *));
                    ImGui::Text("%s", l->name.c_str());
                    ImGui::EndDragDropSource();
                }

                ImRect top_half(row_min, ImVec2(row_max.x, mid_y));
                ImRect bot_half(ImVec2(row_min.x, mid_y), row_max);

                auto DoDropTarget = [&](ImRect rect, bool is_before, ImGuiID id) {
                    if(ImGui::BeginDragDropTargetCustom(rect, id)) {
                        if(ImGui::IsDragDropActive()) {
                            float line_y = is_before ? row_min.y : row_max.y;
                            ImU32 col = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
                            ImGui::GetForegroundDrawList()->AddLine(ImVec2(row_min.x, line_y), ImVec2(row_max.x, line_y), col, 2.5f);
                        }

                        if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("GERBER_LAYER")) {
                            item_to_move = *(gerber_layer **)payload->Data;
                            item_target = l;
                            move_before = is_before;
                        }
                        ImGui::EndDragDropTarget();
                    }
                };

                DoDropTarget(top_half, true, ImGui::GetID("##top"));
                DoDropTarget(bot_half, false, ImGui::GetID("##bot"));

                ImGui::TableSetColumnIndex(1);
                IconCheckbox("##vis", &l->visible, MATSYM_visibility, MATSYM_visibility_off);
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetItemTooltip("Toggle visibility");
                }
                // ImGui::SameLine();
                // IconCheckbox("##inv", &l->invert, MATSYM_invert_colors, MATSYM_invert_colors_off);
                // if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                //     ImGui::SetItemTooltip("Invert layer");
                // }
                // ImGui::SameLine();
                // IconCheckboxTristate("##mode", &l->draw_mode, MATSYM_radio_button_checked, MATSYM_circle, MATSYM_radio_button_off);
                // if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                //     ImGui::SetItemTooltip("Solid/Mixed/Outline only");
                // }
                ImGui::SameLine();
                gpu::colorf4 e(l->fill_color);
                auto color_flags = ImGuiColorEditFlags_NoInputs |    //
                                   ImGuiColorEditFlags_NoLabel |     //
                                   ImGuiColorEditFlags_AlphaBar |    //
                                   ImGuiColorEditFlags_AlphaPreview;
                if(ImGui::ColorEdit4("##clr", e.f, color_flags)) {
                    l->fill_color = (gpu::color)e;
                }
                ImGui::SameLine();
                if(IconButton("##del", MATSYM_close)) {
                    item_to_delete = l;
                }
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetItemTooltip("Delete layer");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        if(item_to_delete) {
            set_active_entity(nullptr);
            select_layer(nullptr);
            if(item_to_delete->job_count.load() == 0) {
                layers.erase(std::remove(layers.begin(), layers.end(), item_to_delete), layers.end());
                delete item_to_delete;
            } else {
                item_to_delete->marked_for_deletion = true;
            }
        } else if(item_to_move && item_target && item_to_move != item_target) {
            auto dragged_it = std::find(layers.begin(), layers.end(), item_to_move);
            auto target_it = std::find(layers.begin(), layers.end(), item_target);
            if(dragged_it != layers.end() && target_it != layers.end()) {
                if(move_before) {
                    target_it = std::next(target_it);
                }
                layers.splice(target_it, layers, dragged_it);
            }
        }

        if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !any_item_hovered) {
            select_layer(nullptr);
        }
    }
    ImGui::End();

    ImGui::Begin("Info");
    {
        double scale = 1.0;
        char const *units_str = "mm";
        if(settings.units == settings::units_inch) {
            scale = 1.0 / 25.4;
            units_str = "in";
        }

        // Mouse position at the top, right-aligned
        {
            vec2d pos = world_mouse_pos.scale(scale);
            std::string mouse_text = std::format("{:8.4f}  {:8.4f}  {}", pos.x, pos.y, units_str);
            float text_width = ImGui::CalcTextSize(mouse_text.c_str()).x;
            float posX = ImGui::GetWindowWidth() - text_width - ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorPosX(posX);
            ImGui::Text("%s", mouse_text.c_str());
        }

        // Measurement distance
        if(measure_dragging || measure_line_visible) {
            double dx = (measure_end_world.x - measure_start_world.x) * scale;
            double dy = (measure_end_world.y - measure_start_world.y) * scale;
            double distance = std::sqrt(dx * dx + dy * dy);
            ImGui::Text("Distance: %.4f %s (dx: %.4f, dy: %.4f)", distance, units_str, dx, dy);
        }

        ImGui::Separator();

        if(active_entity != nullptr && !active_entity_info.empty()) {
            if(ImGui::BeginTable("##entity_info", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                for(auto const &line : active_entity_info) {
                    auto colon = line.find(':');
                    if(colon != std::string::npos && colon < 20) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(line.c_str(), line.c_str() + colon);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(line.c_str() + colon + 1);
                    } else {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(line.c_str());
                        ImGui::TableNextColumn();
                    }
                }
                ImGui::EndTable();
            }
        } else if(selected_layer != nullptr) {
            char const *layer_type_name = gerber_lib::layer_type_name_friendly(selected_layer->layer_type());
            ImGui::Text("%s", selected_layer->name.c_str());
            ImGui::Text("%s, %zu entities", layer_type_name, selected_layer->drawer->entities.size());
        } else {
            ImGui::Text("Select a layer...");
        }
    }
    ImGui::End();

    job_pool::pool_info info = pool.get_info();

    ImGui::Begin("Job Pool");
    {
        ImGui::Text("Active: %5zu, Queued: %5zu", info.active, info.queued);
    }
    ImGui::End();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::update_board_extent()
{
    board_extent = view_rect;
    visible_board_extent = view_rect;

    rect all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
    rect visible_all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };

    bool all_valid{false};
    bool visible_valid{false};
    for(auto layer : layers) {
        rect const &r = layer->extent();
        if(r.is_normalized()) {
            all_valid = true;
            all = all.union_with(layer->extent());
            if(layer_is_visible(layer)) {
                visible_all = visible_all.union_with(layer->extent());
                visible_valid = true;
            }
        }
    }

    if(all_valid) {
        board_extent = all;
        board_center = board_extent.center();
    }

    if(visible_valid) {
        visible_board_extent = visible_all;
        visible_board_center = visible_board_extent.center();
    }
}

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////

void gerber_explorer::set_outline_layer(gerber_layer *new_outline_layer)
{
    for(auto *l : layers) {
        if(l->is_outline_layer && l != new_outline_layer) {
            l->is_outline_layer = false;
            l->got_mask = false;
            l->drawer->got_mask = false;
            l->gpu_resources.ready = false;
            l->drawer->mask.release();
        }
    }
    if(new_outline_layer != nullptr) {
        new_outline_layer->is_outline_layer = true;
        pool.add_job(job_type_create_mask, [new_outline_layer](std::stop_token st) {
            new_outline_layer->job_count.fetch_add(1);
            if(!st.stop_requested()) {
                new_outline_layer->drawer->create_mask();
                new_outline_layer->gpu_resources.ready = false;    // force GPU recreation with mask
            }
            new_outline_layer->job_count.fetch_sub(1);
        });
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_render()
{
    // Tell ImGui not to manage cursor when we're in measure mode
    ImGuiIO &io = ImGui::GetIO();
    if(measure_mode) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        set_cursor(crosshair_cursor);
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }

    // gather up any layers which finished loading
    // if we just got the last one loaded, fit to viewport
    {
        std::lock_guard _(loaded_mutex);
        if(!loaded_layers.empty()) {
            gerber_layer *loaded_layer = loaded_layers.front();
            loaded_layers.pop_front();
            layers.push_front(loaded_layer);

            // Auto-detect outline layer (only if none exists)
            if(get_outline_layer() == nullptr) {
                auto layer_type = loaded_layer->layer_type();
                if(gerber_lib::is_layer_type(layer_type, gerber_lib::layer::type_t::board) ||
                   gerber_lib::is_layer_type(layer_type, gerber_lib::layer::type_t::outline)) {
                    set_outline_layer(loaded_layer);
                }
            }

            layers.sort([](gerber_layer const *a, gerber_layer const *b) { return a->index > b->index; });
            LOG_VERBOSE("Loaded layer \"{}\"", loaded_layer->filename());
            if(loaded_layers.empty()) {
                select_layer(nullptr);
                fit_to_viewport();
            }
        }
    }

    // delete layers whose jobs have finished
    layers.remove_if([](gerber_layer *l) {
        if(l->marked_for_deletion && l->job_count.load() == 0) {
            delete l;
            return true;
        }
        return false;
    });

    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // On first run (no imgui.ini), set up a default docking layout
    if(frames == 0 && !std::filesystem::exists(ImGui::GetIO().IniFilename)) {

        ImGui::DockBuilderRemoveNodeChildNodes(dockspace_id);

        // Top toolbar (full width)
        ImGuiID dock_top_id;
        ImGuiID dock_rest_id;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.04f, &dock_top_id, &dock_rest_id);

        // Bottom info bar (full width)
        ImGuiID dock_bottom_id;
        ImGuiID dock_middle_id;
        ImGui::DockBuilderSplitNode(dock_rest_id, ImGuiDir_Down, 0.06f, &dock_bottom_id, &dock_middle_id);

        // Left files panel (sandwiched between toolbar and info)
        ImGuiID dock_left_id;
        ImGuiID dock_center_id;
        ImGui::DockBuilderSplitNode(dock_middle_id, ImGuiDir_Left, 0.2f, &dock_left_id, &dock_center_id);

        ImGui::DockBuilderDockWindow("Toolbar", dock_top_id);
        ImGui::DockBuilderDockWindow("Info", dock_bottom_id);
        ImGui::DockBuilderDockWindow("Files", dock_left_id);
#ifdef _DEBUG
        ImGui::DockBuilderDockWindow("Job Pool", dock_left_id);
#endif
        // Hide tab bars on nodes with a single window
        auto hide_tab_bar = [](ImGuiID id) {
            ImGuiDockNode *n = ImGui::DockBuilderGetNode(id);
            if(n) {
                n->SetLocalFlags(n->LocalFlags | ImGuiDockNodeFlags_AutoHideTabBar);
            }
        };
        hide_tab_bar(dock_top_id);
        hide_tab_bar(dock_bottom_id);
        hide_tab_bar(dock_left_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGuiDockNode *central_node = ImGui::DockBuilderGetCentralNode(dockspace_id);

    rect old_viewport_rect = viewport_rect;

    if(central_node) {
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        ImVec2 pos = ImVec2(central_node->Pos.x - main_viewport->Pos.x, central_node->Pos.y - main_viewport->Pos.y);
        ImVec2 size = central_node->Size;
        viewport_xpos = (int)pos.x;
        viewport_ypos = (int)pos.y;
        viewport_width = (int)size.x;
        viewport_height = (int)size.y;
    } else {
        // this should never happen
        viewport_xpos = 0;
        viewport_ypos = 0;
        viewport_width = window_width;
        viewport_width = window_height;
    }
    viewport_rect = rect(vec2d(viewport_xpos, viewport_ypos), vec2d(viewport_xpos + viewport_width, viewport_ypos + viewport_height));
    viewport_size = viewport_rect.size();

    bool viewport_changed = viewport_rect.min_pos.x != old_viewport_rect.min_pos.x || viewport_rect.min_pos.y != old_viewport_rect.min_pos.y ||
                            viewport_rect.max_pos.x != old_viewport_rect.max_pos.x || viewport_rect.max_pos.y != old_viewport_rect.max_pos.y;

    vec2d new_viewport_size;
    new_viewport_size.x = viewport_width;
    new_viewport_size.y = viewport_height;

    vec2d scale_factor = new_viewport_size.divide(viewport_size);

    vec2d new_view_size = view_rect.size().multiply(scale_factor);
    view_rect.max_pos = view_rect.min_pos.add(new_view_size);

    if(viewport_changed) {
        if(should_fit_to_viewport) {
            fit_to_viewport();
            view_rect = target_view_rect;
            zoom_anim = false;
        } else {
            // viewport dimensions changed, updated view_rect accordingly
            vec2d scale = old_viewport_rect.size().divide(viewport_rect.size());
            view_rect.max_pos = view_rect.min_pos.add(view_rect.size().divide(scale));
            zoom_anim = false;
        }
    }

    int framebuffer_width;
    int framebuffer_height;
    get_framebuffer_size(&framebuffer_width, &framebuffer_height);

    handle_mouse();

    // if there are outstanding jobs running, it's active (so we see the result when they complete)
    if(pool.get_info().active != 0) {
        set_active();
    }

    update_view_rect();

    if(window_width == 0 || window_height == 0) {
        return;
    }

    view_scale = viewport_size.divide(view_rect.size());

    // --- explicit retesselation (quality slider changed) ---
    if(retesselate) {
        retesselate = false;
        pool.abort_jobs(job_type_tesselate);
        // busy-wait for explicit changes (user expects immediate result)
        while(pool.get_info().active != 0) {}
        double ppwu = settings.dynamic_tesselation ? std::min(view_scale.x, view_scale.y) : 0;
        for(auto l : layers) {
            if(l->marked_for_deletion) continue;
            auto options = l->is_outline_layer ? tesselation_options_force_outline : tesselation_options_none;
            tesselate_layer(l, options, ppwu);
        }
        last_tess_ppwu = ppwu;
        dynamic_tess_pending = false;
    }

    // --- dynamic retesselation (zoom-driven) ---
    if(settings.dynamic_tesselation && !layers.empty()) {
        double ppwu = std::min(view_scale.x, view_scale.y);
        double ratio = (last_tess_ppwu > 0) ? ppwu / last_tess_ppwu : 0;
        bool needs_retess = (ratio < 0.8 || ratio > 1.25) || last_tess_ppwu == 0;
        bool scale_changing = (ppwu != prev_frame_ppwu);
        prev_frame_ppwu = ppwu;

        if(needs_retess) {
            if(scale_changing) {
                // scale is actively changing (scrolling) — keep resetting the debounce timer
                dynamic_tess_debounce_start = get_time();
                dynamic_tess_pending = true;
            } else if(!dynamic_tess_pending) {
                // scale jumped but is now stable — start the timer
                dynamic_tess_debounce_start = get_time();
                dynamic_tess_pending = true;
            }
        }
        if(dynamic_tess_pending) {
            set_active();    // keep the main loop polling so we don't miss the timer
            double elapsed = get_time() - dynamic_tess_debounce_start;
            if(elapsed >= 0.2) {
                dynamic_tess_pending = false;
                pool.abort_jobs(job_type_tesselate);
                // non-blocking: don't wait for old jobs, old drawers remain visible
                for(auto l : layers) {
                    if(l->marked_for_deletion || l->is_outline_layer) continue;
                    tesselate_layer(l, tesselation_options_none, ppwu);
                }
                last_tess_ppwu = ppwu;
            }
        }
    }

    // get bounding rect/center of all layers (for flip)
    update_board_extent();

    flip_xy = { settings.flip_x ? -1.0 : 1.0, settings.flip_y ? -1.0 : 1.0 };

    ortho_screen_matrix = gpu::make_ortho(viewport_width, viewport_height);

    // world matrix has optional x/y flip around board center
    gpu::matrix flip_m = gpu::make_identity();
    flip_m.m[0] = (float)flip_xy.x;
    flip_m.m[5] = (float)flip_xy.y;
    flip_m.m[12] = (float)(board_center.x - flip_xy.x * board_center.x);
    flip_m.m[13] = (float)(board_center.y - flip_xy.y * board_center.y);

    gpu::matrix view_m = gpu::make_identity();
    view_m.m[0] = (float)view_scale.x;
    view_m.m[5] = (float)view_scale.y;
    view_m.m[12] = (float)(-(view_rect.min_pos.x * view_scale.x));
    view_m.m[13] = (float)(-(view_rect.min_pos.y * view_scale.y));
    gpu::matrix temp = matrix_multiply(view_m, flip_m);
    world_matrix = matrix_multiply(ortho_screen_matrix, temp);

    // Flip Y in clip space: gerber world coords are Y-up, GPU clip space is Y-down
    gpu::matrix clip_y_flip = gpu::make_identity();
    clip_y_flip.m[5] = -1.0f;
    world_matrix = matrix_multiply(clip_y_flip, world_matrix);

    //////////////////////////////////////////////////////////////////////
    // Draw stuff

    // render target resize is handled in gpu_render()

    // update which drawer being used (in case retesselation happened)
    {
        std::lock_guard l(layer_drawer_mutex);
        for(auto &layer : layers) {
            gerber_drawer *old_drawer = layer->drawer;
            layer->drawer = &layer->drawers[layer->current_drawer];
            layer->got_mask = layer->drawer->got_mask;
            if(layer->drawer != old_drawer) {
                layer->gpu_resources.ready = false;
            }
        }
    }

    // draw the layers

    // 1. gather all the visible layers for current view mode (and update their drawer pointers)
    gerber_layer *outline_layer{ nullptr };
    std::vector<gerber_layer *> ordered_layers;
    ordered_layers.reserve(layers.size());
    for(auto const layer : layers) {
        if(layer_is_visible(layer) && !layer->drawer->entities.empty()) {
            ordered_layers.push_back(layer);
        }
        // first valid outline layer is it
        if(outline_layer == nullptr && layer->is_outline_layer && layer->got_mask) {
            outline_layer = layer;
        }
    }

    // 2. sort them, based on current view mode
    if(settings.board_view != board_view_all) {
        std::sort(ordered_layers.begin(), ordered_layers.end(), [this](gerber_layer const *a, gerber_layer const *b) {
            using namespace gerber_lib;
            layer::type_t drill_ordered = layer::type_t::drill_top;
            if(settings.board_view == board_view_bottom) {
                drill_ordered = layer::type_t::drill_bottom;
                std::swap(a, b);
            }
            int ta = a->file.layer_type;
            int tb = b->file.layer_type;
            if(is_layer_type(ta, layer::type_t::drill)) {
                ta = drill_ordered;
            }
            if(is_layer_type(tb, layer::type_t::drill)) {
                tb = drill_ordered;
            }
            return ta > tb;
        });
    }

    double t = get_time();

    gpu_render();
    ui();
    last_frame_cpu_time = get_time() - t;
    if(zoom_anim) {
        SDL_Event e{}; e.type = SDL_EVENT_USER; SDL_PushEvent(&e);
    }
}

