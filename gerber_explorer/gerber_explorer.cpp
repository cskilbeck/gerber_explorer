#include <windows.h>

#include <filesystem>
#include <expected>

#define GLFW_EXPOSE_NATIVE_WIN32
#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"

#include <nfd.h>

#include "gerber_explorer.h"

#include "gerber_aperture.h"
#include "gerber_net.h"
#include "gl_matrix.h"
#include "gl_colors.h"
#include "util.h"

#include "assets/matsym_codepoints_utf8.h"

#include "GLFW/glfw3native.h"

LOG_CONTEXT("gerber_explorer", debug);

namespace
{
    using namespace gerber;

    enum board_view_t
    {
        board_view_all = 0,
        board_view_top = 1,
        board_view_bottom = 2,
        board_view_num_views = 3
    };

    char const *const board_view_names[board_view_num_views] = { "All", "Front", "Back" };

    using gerber_lib::rect;
    using gerber_lib::vec2d;
    using gerber_lib::vec2f;

    long long const zoom_lerp_time_ms = 700;

    double const drag_select_offset_start_distance = 16;

    gl::color layer_colors[] = { (gl::color)gl::colors::dark_green, (gl::color)gl::colors::dark_cyan,     (gl::color)gl::colors::green,
                                 (gl::color)gl::colors::lime_green, (gl::color)gl::colors::antique_white, (gl::color)gl::colors::corn_flower_blue,
                                 (gl::color)gl::colors::gold };

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
        gl::color color;
    };

    //////////////////////////////////////////////////////////////////////

    bool constexpr layer_normal = false;
    bool constexpr layer_inverted = true;

    layer_defaults_t layer_defaults[] = {
        { gerber_lib::layer::unknown, layer_normal, layer_order_t::other, gl::colors::yellow },
        { gerber_lib::layer::vcut, layer_normal, layer_order_t::other, gl::colors::magenta },
        { gerber_lib::layer::board, layer_normal, layer_order_t::other, gl::colors::black },
        { gerber_lib::layer::outline, layer_normal, layer_order_t::other, gl::colors::black },
        { gerber_lib::layer::mechanical, layer_normal, layer_order_t::other, gl::colors::cyan },
        { gerber_lib::layer::info, layer_normal, layer_order_t::other, gl::colors::white },
        { gerber_lib::layer::keepout, layer_normal, layer_order_t::other, gl::colors::magenta },
        { gerber_lib::layer::drill, layer_normal, layer_order_t::drill, gl::colors::black },
        { gerber_lib::layer::paste_top, layer_normal, layer_order_t::top_outer, gl::colors::silver },
        { gerber_lib::layer::overlay_top, layer_normal, layer_order_t::top_outer, gl::colors::white },
        { gerber_lib::layer::soldermask_top, layer_inverted, layer_order_t::top_outer, gl::set_alpha(gl::colors::dark_green, 0.75f) },
        { gerber_lib::layer::copper_top, layer_normal, layer_order_t::top_copper, 0xFF34AAAC },
        { gerber_lib::layer::copper_inner, layer_normal, layer_order_t::inner_copper, 0xFF34AAAC },
        { gerber_lib::layer::copper_bottom, layer_normal, layer_order_t::bottom_copper, 0xFF34AAAC },
        { gerber_lib::layer::soldermask_bottom, layer_inverted, layer_order_t::bottom_outer, gl::set_alpha(gl::colors::dark_green, 0.75f) },
        { gerber_lib::layer::overlay_bottom, layer_normal, layer_order_t::bottom_outer, gl::colors::white },
        { gerber_lib::layer::paste_bottom, layer_normal, layer_order_t::bottom_outer, gl::colors::silver },
    };

    //////////////////////////////////////////////////////////////////////

    bool is_bottom_layer(layer_order_t o)
    {
        return o == layer_order_t::bottom_outer || o == layer_order_t::bottom_copper || o == layer_order_t::drill;
    }

    //////////////////////////////////////////////////////////////////////

    bool is_top_layer(layer_order_t o)
    {
        return o == layer_order_t::top_outer || o == layer_order_t::top_copper || o == layer_order_t::drill;
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

gl::solid_program gerber_explorer::solid_program{};
gl::color_program gerber_explorer::color_program{};
gl::layer_program gerber_explorer::layer_program{};
gl::blit_program gerber_explorer::blit_program{};
gl::selection_program gerber_explorer::selection_program{};
gl::arc_program gerber_explorer::arc_program{};
gl::line2_program gerber_explorer::line2_program{};

//////////////////////////////////////////////////////////////////////
// If zoom_anim or any jobs in the pool, it's not idle
// But also... suppress idleness for a couple of frames

bool gerber_explorer::is_idle()
{
    static int idle_frames = 0;
    job_pool::pool_info info = pool.get_info();
    if(info.active == 0 && info.queued == 0 && !zoom_anim) {
        idle_frames += 1;
    } else {
        idle_frames = 0;
    }
    return idle_frames > 3;
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
    } else if(selected_layer != nullptr && selected_layer->is_valid()) {
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
    } else {
        should_fit_to_viewport = true;
        update_board_extent();
        zoom_to_rect(board_extent);
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
    vec2d bl = view_rect.min_pos.subtract(zoom_pos).multiply(zoom_scale).add(zoom_pos);
    vec2d tr = view_rect.max_pos.subtract(zoom_pos).multiply(zoom_scale).add(zoom_pos);
    view_rect = { bl, tr };
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::update_view_rect()
{
    if(zoom_anim) {

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
    if((action == GLFW_PRESS || action == GLFW_REPEAT)) {
        if(mods == 0) {
            switch(key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_V:
                next_view();
                break;
            case GLFW_KEY_F:
                fit_to_viewport();
                break;
            case GLFW_KEY_X:
                settings.flip_x = !settings.flip_x;
                break;
            case GLFW_KEY_Y:
                settings.flip_y = !settings.flip_y;
                break;
            case GLFW_KEY_W:
                settings.wireframe = !settings.wireframe;
                break;
            case GLFW_KEY_A:
                settings.show_axes = !settings.show_axes;
                break;
            case GLFW_KEY_E:
                settings.show_extent = !settings.show_extent;
                break;
            }
        } else if(mods & GLFW_MOD_CONTROL) {
            switch(key) {
            case GLFW_KEY_O:
                file_open();
                break;
            case GLFW_KEY_S: {
                auto save_path = save_file_dialog();
                if(save_path.has_value()) {
                    save_settings(save_path.value());
                }
            } break;
            case GLFW_KEY_L: {
                auto load_path = load_file_dialog();
                if(load_path.has_value()) {
                    load_settings(load_path.value());
                }
            } break;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_scroll(double xoffset, double yoffset)
{
    double scale_factor = (yoffset > 0) ? 0.9 : 1.1;
    zoom_at_point(world_pos_from_viewport_pos(mouse_pos), scale_factor);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_button(int button, int action, int mods)
{
    switch(action) {
    case GLFW_PRESS:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if((mods & GLFW_MOD_CONTROL) != 0) {
                set_mouse_mode(mouse_drag_zoom_select);
            } else {
                set_mouse_mode(mouse_drag_maybe_select);
            }
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_pan);
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_zoom);
            break;
        }
        break;
    case GLFW_RELEASE:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if(mouse_mode == mouse_drag_zoom_select) {
                rect drag_rect_corrected = correct_aspect_ratio(viewport_rect.aspect_ratio(), drag_rect, aspect_expand);
                vec2d mn = drag_rect_corrected.min_pos;
                vec2d mx = drag_rect_corrected.max_pos;
                rect d = rect{ mn, mx }.normalize();
                if(d.width() > 2 && d.height() > 2) {
                    zoom_to_rect(world_rect_from_viewport_rect({ mn, mx }));
                    should_fit_to_viewport = false;
                }
            } else if(mouse_mode == mouse_drag_select && selected_layer != nullptr) {
                selected_layer->drawer->select_hovered_entities();
            }
            set_mouse_mode(mouse_drag_none);
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_none);
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_none);
            break;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_move(double xpos, double ypos)
{
    mouse_pos = { xpos - viewport_xpos, viewport_height - (ypos - viewport_ypos) };
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
                double factor = (d.x + d.y) * 0.01;
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
            // Just hovering, highlight entities under the mouse if selected_layer != nullptr
            if(selected_layer != nullptr) {
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
    pool.shut_down();
    NFD_Quit();
    save_settings(config_path(app_name, settings_filename));
    gl_window::on_closed();
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
    gl_window::on_window_size(w, h);
    window_width = w;
    window_height = h;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_window_refresh()
{
    gl_window::on_window_refresh();
    on_frame();
    if(should_fit_to_viewport) {
        view_rect = target_view_rect;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::close_all_layers()
{
    LOG_INFO("CLOSE ALL");
    select_layer(nullptr);
    while(!layers.empty()) {
        auto l = layers.front();
        layers.pop_front();
        delete l;
    }
    select_layer(nullptr);
}

//////////////////////////////////////////////////////////////////////

bool gerber_explorer::layer_is_visible(gerber_layer const *layer) const
{
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

void gerber_explorer::save_settings(std::filesystem::path const &path)
{
    window_state = get_window_state();
    settings.window_width = window_state.width;
    settings.window_height = window_state.height;
    settings.window_xpos = window_state.x;
    settings.window_ypos = window_state.y;
    settings.window_maximized = window_state.isMaximized;

    settings.files.clear();
    int index = 1;
    for(auto it = layers.crbegin(); it != layers.crend(); ++it) {
        gerber_layer *layer = *it;
        if(layer->is_valid()) {
            settings.files.emplace_back(layer->filename(), gl::color_to_string(layer->fill_color), layer->visible, layer->invert, index);
            index += 1;
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
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;

    case mouse_drag_pan:
        drag_mouse_start_pos = mouse_pos;
        zoom_anim = false;
        break;

    case mouse_drag_zoom:
        zoom_anim = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mouse_world_pos = world_pos_from_viewport_pos(mouse_pos);
        drag_mouse_cur_pos = mouse_pos;
        drag_mouse_start_pos = mouse_pos;
        // ignore the next 2 mouse moves because sometimes they are kind of
        // random, something to do with GLFW_CURSOR_DISABLED/NORMAL
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
        HWND hwnd = glfwGetWin32Window(window);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL2, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }
#endif

    pool.start_workers();

    NFD_Init();

    glfwGetWindowSize(window, &window_width, &window_height);
    window_size.x = window_width;
    window_size.y = window_height;
    view_rect = { { 0, 0 }, { 10, 10 } };

    solid_program.init();
    color_program.init();
    layer_program.init();
    blit_program.init();
    selection_program.init();
    arc_program.init();
    line2_program.init();

    overlay.init();

    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &max_multisamples);

    LOG_INFO("MAX GL Multisamples: {}", max_multisamples);

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
                    add_gerber(settings::layer_t{ outPath, gl::colorf4(layer_colors[next_layer_color]).to_string(), true, false, -1 });
                    NFD_FreePathU8(outPath);
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

std::expected<std::filesystem::path, std::error_code> gerber_explorer::save_file_dialog()
{
    nfdu8char_t *path;
    nfdresult_t result = NFD_SaveDialogU8(&path, nullptr, 0, nullptr, "settings.json");
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

void gerber_explorer::tesselate_layer(gerber_layer *layer, tesselation_options_t options)
{
    pool.add_job(job_type_tesselate, [this, layer, options](std::stop_token st) {
        bool force_outline = (options & tesselation_options_force_outline) != 0;
        int d = 1 - layer->current_drawer;
        gl_drawer *other_drawer = &layer->drawers[d];
        using namespace gerber_lib;
        auto layer_type = layer->layer_type();
        layer->is_outline_layer = force_outline || is_layer_type(layer_type, layer::type_t::board) || is_layer_type(layer_type, layer::type_t::outline);
        other_drawer->tesselation_quality = settings.tesselation_quality;
        other_drawer->set_gerber(layer->file);
        if(layer->is_outline_layer) {
            other_drawer->create_mask();
        }
        {
            std::lock_guard l(layer_drawer_mutex);
            layer->current_drawer = d;
        }
    });
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_gerber(settings::layer_t const &layer_to_load)
{
    gerber_lib::gerber_file *g = new gerber_lib::gerber_file();
    gerber_lib::gerber_error_code err = g->parse_file(layer_to_load.filename.c_str());
    if(err == gerber_lib::ok) {
        gerber_layer *layer = new gerber_layer();
        layer->init();
        layer->file = g;
        layer->index = layer_to_load.index;
        layer->name = std::format("{}", std::filesystem::path(g->filename).filename().string());
        layer->visible = layer_to_load.visible;
        layer->clear_color = gl::colors::black;
        layer->drawer = &layer->drawers[0];

        layer_defaults_t d = get_defaults_for_layer_type(g->layer_type);
        if(layer->index == -1) {
            layer->index = g->layer_type;
            LOG_DEBUG("{}:{} ({})", layer->name, g->image.info.polarity, d.is_inverted);
            if(g->image.info.polarity == gerber_lib::polarity_unspecified) {
                layer->invert = d.is_inverted;
            } else {
                layer->invert = g->image.info.polarity == gerber_lib::polarity_negative;
            }
            layer->fill_color = d.color;
        } else {
            layer->invert = layer_to_load.inverted;
            layer->fill_color = gl::color_from_string(layer_to_load.color);
        }
        next_index = std::max(layer->index + 1, next_index);
        layer->layer_order = d.layer_order;

        LOG_DEBUG("Finished loading {}, \"{}\"", layer->index, layer_to_load.filename);

        pool.add_job(job_type_tesselate, [layer, this]([[maybe_unused]] std::stop_token st) {
            using namespace gerber_lib;
            layer->drawer->tesselation_quality = settings.tesselation_quality;
            layer->drawer->set_gerber(layer->file);    // <----- TESSELATION HAPPENS HERE !!!!!
            auto layer_type = layer->layer_type();
            layer->is_outline_layer = is_layer_type(layer_type, layer::type_t::board) || is_layer_type(layer_type, layer::type_t::outline);
            LOG_INFO("Tesselated ({}:{}) {}", layer_type_name(layer_type), layer->is_outline_layer, layer->filename());
            if(layer->is_outline_layer) {
                layer->drawer->create_mask();
            }

            // inform main thread that there's a new layer available and wait for it to pick it up
            {
                std::lock_guard loaded_lock(loaded_mutex);
                loaded_layers.push_back(layer);
            }
            bool loaded = false;
            while(!loaded) {
                {
                    std::lock_guard loaded_lock(loaded_mutex);
                    loaded = loaded_layers.empty();
                    glfwPostEmptyEvent();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        });
    } else {
        LOG_ERROR("Error loading {} ({})", layer_to_load.filename, gerber_lib::get_error_text(err));
    }
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
        active_entity_description.clear();
        return;
    }
    active_entity->flags |= entity_flags_t::active;
    using namespace gerber_lib;

    gerber_net *net = active_entity->net;
    gerber_level *level = net->level;
    gerber_polarity polarity = level->polarity;
    gerber_aperture_type aperture_type{ aperture_type_none };

    std::string interpolation{ "" };
    if(net->aperture_state == aperture_state_on && net->interpolation_method < interpolation_region_start) {
        interpolation = std::format(" {} interpolation,", net->interpolation_method);
    }

    std::string state{ "" };
    if(net->aperture_state != aperture_state_on) {
        state = std::format("{}", net->aperture_state);
    }

    std::string description{ "?" };
    if(net->aperture != 0) {
        auto apertures = selected_layer->file->image.apertures;
        auto it = apertures.find(net->aperture);
        if(it != apertures.end()) {
            gerber_aperture *aperture = it->second;
            aperture_type = aperture->aperture_type;
            if(aperture_type >= aperture_type_macro) {
                gerber_aperture_macro *macro = aperture->aperture_macro;
                description = std::format("macro {}", macro->name);
            } else {
                description = std::format("aperture {} ({})", net->aperture, aperture_type);
            }
        } else {
            description = std::format("?unknown aperture {}?", net->aperture);
        }
    } else if(net->num_region_points != 0) {
        description = std::format("region ({} points)", net->num_region_points);
    }

    int x = selected_layer->drawer->entity_flags.data()[active_entity->entity_id()];
    active_entity_description =
        std::format("Entity {}:{}{} {} polarity ({}) flags: {} ({})", net->entity_id, state, interpolation, polarity, description, active_entity->flags, x);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::ui()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Open Layers", "Ctrl-O", nullptr)) {
                file_open();
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Save Settings", "Ctrl-S", nullptr)) {
                auto save_path = save_file_dialog();
                if(save_path.has_value()) {
                    save_settings(save_path.value());
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
                glfwSetWindowShouldClose(window, true);
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
                }
                if(ImGui::MenuItem("Inch", "", settings.units == settings::units_inch)) {
                    settings.units = settings::units_inch;
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Multisamples")) {
                ImGui::SliderInt("##multisamples", &settings.multisamples, 1, max_multisamples, "%d");
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Tesselation")) {
                if(ImGui::SliderInt("##tesselation",
                                    &settings.tesselation_quality,
                                    tesselation_quality::low,
                                    tesselation_quality::high,
                                    tesselation_quality_name(settings.tesselation_quality))) {
                    retesselate = true;
                }
                if(ImGui::Checkbox("Dynamic", &settings.dynamic_tesselation)) {
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

        // 1. Define your text
#if defined(_DEBUG)
        static int frames = 0;
        frames += 1;
        std::string text = std::format("Frame {:07d} {:5.2f}ms {:5.2f}ms", frames, last_frame_elapsed_time * 1000.0, last_frame_cpu_time * 1000.0);
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

                if(ImGui::Selectable(l->name.c_str(), is_selected, flags, ImVec2(0, row_height))) {
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
                    bool was_outline = l->is_outline_layer;
                    ImGui::MenuItem("Outline", nullptr, &l->is_outline_layer);
                    if(was_outline != l->is_outline_layer) {
                        if(l->is_outline_layer) {
                            tesselate_layer(l, tesselation_options_force_outline);
                        } else {
                            l->got_mask = false;
                            l->drawer->got_mask = false;
                            l->drawer->mask.release_gl_resources();
                            l->drawer->mask.release();
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
                gl::colorf4 e(l->fill_color);
                auto color_flags = ImGuiColorEditFlags_NoInputs |    //
                                   ImGuiColorEditFlags_NoLabel |     //
                                   ImGuiColorEditFlags_AlphaBar |    //
                                   ImGuiColorEditFlags_AlphaPreview;
                if(ImGui::ColorEdit4("##clr", e.f, color_flags)) {
                    l->fill_color = (gl::color)e;
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
            layers.erase(std::remove(layers.begin(), layers.end(), item_to_delete), layers.end());
            select_layer(nullptr);
            delete item_to_delete;
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
        if(active_entity != nullptr) {
            ImGui::Text("%s", active_entity_description.c_str());
        } else if(selected_layer != nullptr) {
            char const *layer_type_name = gerber_lib::layer_type_name_friendly(selected_layer->layer_type());
            ImGui::Text("%s - %s (%llu entities) (outline: %d) (got_mask: %d)",
                        selected_layer->name.c_str(),
                        layer_type_name,
                        selected_layer->drawer->entities.size(),
                        selected_layer->is_outline_layer,
                        selected_layer->got_mask);
        } else {
            ImGui::Text("Select a layer...");
        }
        ImGui::SameLine();
        double scale = 1.0;
        char const *units_str = "mm";
        if(settings.units == settings::units_inch) {
            scale = 1.0 / 2.54;
            units_str = "in";
        }
        vec2d pos = world_mouse_pos.scale(scale);
        std::string text = std::format("{:8.4f} {:8.4f} {}", pos.x, pos.y, units_str);
        float text_width = ImGui::CalcTextSize(text.c_str()).x;
        float posX = ImGui::GetWindowWidth() - text_width - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(posX);
        ImGui::Text("%s", text.c_str());
    }
    ImGui::End();

#ifdef _DEBUG

    job_pool::pool_info info = pool.get_info();

    ImGui::Begin("Job Pool");
    {
        ImGui::Text("Active: %5llu, Queued: %5llu", info.active, info.queued);
    }
    ImGui::End();

#endif
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::update_board_extent()
{
    rect all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
    for(auto layer : layers) {
        if(layer_is_visible(layer)) {
            all = all.union_with(layer->extent());
        }
    }
    board_extent = all;
    board_center = all.center();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::blend_layer(gl::color color_fill, gl::color color_other, int num_samples)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    GL_CHECK(glViewport(viewport_xpos, window_height - (viewport_height + viewport_ypos), viewport_width, viewport_height));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    blit_program.activate();
    layer_render_target.bind_textures();

    if(num_samples == 0) {
        num_samples = layer_render_target.num_samples;
    }

    GL_CHECK(glUniform4fv(blit_program.u_fill_color, 1, gl::colorf4(color_fill).f));
    GL_CHECK(glUniform4fv(blit_program.u_other_color, 1, gl::colorf4(color_other).f));
    GL_CHECK(glUniform1i(blit_program.u_num_samples, num_samples));
    GL_CHECK(glUniform1i(blit_program.u_cover_sampler, 0));

    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendEquation(GL_FUNC_ADD));
    GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::blend_selection(gl::color red, gl::color green, gl::color blue, int num_samples)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    GL_CHECK(glViewport(viewport_xpos, window_height - (viewport_height + viewport_ypos), viewport_width, viewport_height));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    selection_program.activate();
    layer_render_target.bind_textures();

    if(num_samples == 0) {
        num_samples = layer_render_target.num_samples;
    }

    GL_CHECK(glUniform4fv(selection_program.u_red_color, 1, gl::colorf4(red).f));
    GL_CHECK(glUniform4fv(selection_program.u_green_color, 1, gl::colorf4(green).f));
    GL_CHECK(glUniform4fv(selection_program.u_blue_color, 1, gl::colorf4(blue).f));
    GL_CHECK(glUniform1i(selection_program.u_num_samples, num_samples));
    GL_CHECK(glUniform1i(selection_program.u_cover_sampler, 0));

    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendEquation(GL_FUNC_ADD));
    GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_render()
{
    // gather up any layers which finished loading
    // if we just got the last one loaded, fit to viewport
    {
        std::lock_guard _(loaded_mutex);
        if(!loaded_layers.empty()) {
            gerber_layer *loaded_layer = loaded_layers.front();
            loaded_layers.pop_front();
            layers.push_front(loaded_layer);
            layers.sort([](gerber_layer const *a, gerber_layer const *b) { return a->index > b->index; });
            LOG_INFO("Loaded layer \"{}\"", loaded_layer->filename());
            if(loaded_layers.empty()) {
                select_layer(nullptr);
                fit_to_viewport();
            }
        }
    }

    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
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
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

    handle_mouse();

    glLineWidth(1.0f);

    ui();

    if(retesselate) {
        retesselate = false;
        pool.abort_jobs(job_type_tesselate);
        while(true) {
            job_pool::pool_info info = pool.get_info();
            if(info.active == 0) {
                break;
            }
        }
        for(auto l : layers) {
            auto options = tesselation_options_none;
            if(l->is_outline_layer) {
                options = tesselation_options_force_outline;
            }
            tesselate_layer(l, options);
        }
    }

    update_view_rect();

    if(window_width == 0 || window_height == 0) {
        return;
    }

    view_scale = viewport_size.divide(view_rect.size());

    // get bounding rect/center of all layers (for flip)
    update_board_extent();

    flip_xy = { settings.flip_x ? -1.0 : 1.0, settings.flip_y ? -1.0 : 1.0 };

    ortho_screen_matrix = gl::make_ortho(viewport_width, viewport_height);

    // world matrix has optional x/y flip around board center
    gl::matrix flip_m = gl::make_identity();
    flip_m.m[0] = (float)flip_xy.x;
    flip_m.m[5] = (float)flip_xy.y;
    flip_m.m[12] = (float)(board_center.x - flip_xy.x * board_center.x);
    flip_m.m[13] = (float)(board_center.y - flip_xy.y * board_center.y);
    gl::matrix view_m = gl::make_identity();
    view_m.m[0] = (float)view_scale.x;
    view_m.m[5] = (float)view_scale.y;
    view_m.m[12] = (float)(-(view_rect.min_pos.x * view_scale.x));
    view_m.m[13] = (float)(-(view_rect.min_pos.y * view_scale.y));
    gl::matrix temp = matrix_multiply(view_m, flip_m);
    world_matrix = matrix_multiply(ortho_screen_matrix, temp);

    //////////////////////////////////////////////////////////////////////
    // Draw stuff

    // resize the offscreen render target if the window size changed
    if(layer_render_target.width != viewport_width || layer_render_target.height != viewport_height ||
       layer_render_target.num_samples != settings.multisamples) {
        layer_render_target.cleanup();
        layer_render_target.init(viewport_width, viewport_height, settings.multisamples, 1);
    }

    // update which drawer being used (in case retesselation happened)
    {
        std::lock_guard l(layer_drawer_mutex);
        for(auto &layer : layers) {
            gl_drawer *old_drawer = layer->drawer;
            layer->drawer = &layer->drawers[layer->current_drawer];
            layer->got_mask = layer->drawer->got_mask;
            if(layer->drawer != old_drawer) {
                old_drawer->release_gl_resources();
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
            outline_layer->drawer->mask.create_gl_resources();
        }
    }

    // 2. sort them, based on current view mode
    if(settings.board_view != board_view_all) {
        std::sort(ordered_layers.begin(), ordered_layers.end(), [this](gerber_layer const *a, gerber_layer const *b) {
            using namespace gerber_lib;
            layer::type_t drill_ordered = layer::type_t::drill_top;
            layer::type_t pads_ordered = layer::type_t::pads_top;
            if(settings.board_view == board_view_bottom) {
                drill_ordered = layer::type_t::drill_bottom;
                pads_ordered = layer::type_t::pads_bottom;
                std::swap(a, b);
            }
            int ta = a->file->layer_type;
            int tb = b->file->layer_type;
            if(is_layer_type(ta, layer::type_t::drill)) {
                ta = drill_ordered;
            } else if(is_layer_type(ta, layer::type_t::pads)) {
                ta = pads_ordered;
            }
            if(is_layer_type(tb, layer::type_t::drill)) {
                tb = drill_ordered;
            } else if(is_layer_type(tb, layer::type_t::pads)) {
                tb = pads_ordered;
            }
            return ta > tb;
        });
    }

    GL_CHECK(glClearColor(settings.background_color.r, settings.background_color.g, settings.background_color.b, 1.0f));

    double t = get_time();

    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glDisable(GL_CULL_FACE));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    if(ordered_layers.empty()) {
        GL_CHECK(glViewport(0, 0, viewport_width, viewport_height));
        GL_CHECK(glClearColor(0, 0, 0, 0));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    }

    // 4. draw them in order
    for(auto it : ordered_layers) {
        gerber_layer &layer = *it;
        layer.drawer->create_gl_resources();
        layer.drawer->update_flags_buffer();

        if(settings.wireframe) {
            GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
        } else {
            GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
        }

        layer_render_target.bind_framebuffer();
        GL_CHECK(glViewport(0, 0, viewport_width, viewport_height));

        // if layer is inverted
        uint8_t fill_flag = entity_flags_t::fill;
        uint8_t clear_flag = entity_flags_t::clear;

        if(layer.invert) {
            // and we have no board outline mask layer
            if(outline_layer == nullptr) {
                // then just clear the whole render target to red
                GL_CHECK(glClearColor(1, 0, 0, 0));
                GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
            } else {
                // else clear the render target then draw the board mask in red
                GL_CHECK(glClearColor(0, 0, 0, 0));
                GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
                solid_program.activate();
                GL_CHECK(glUniformMatrix4fv(solid_program.u_transform, 1, false, world_matrix.m));
                set_uniform_4f(solid_program.u_color, 1, 0, 0, 1);
                outline_layer->drawer->mask.draw();
            }
            // either way, swap clear/fill flags
            std::swap(fill_flag, clear_flag);
        } else {
            // else clear render target to black
            GL_CHECK(glClearColor(0, 0, 0, 0));
            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
        }
        uint8_t draw_flags = entity_flags_t::fill | entity_flags_t::clear;
        layer.drawer->fill(world_matrix, fill_flag, clear_flag, 0, draw_flags);
        blend_layer(layer.fill_color, gl::colors::black, settings.multisamples);
    }

    // draw overlay/ouline of selected & hovered entities in selected layer on top of all other layers

    if(selected_layer != nullptr && !selected_layer->drawer->entities.empty()) {
        layer_render_target.bind_framebuffer();
        GL_CHECK(glViewport(0, 0, viewport_width, viewport_height));
        GL_CHECK(glClearColor(0, 0, 0, 0));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
        uint8_t draw_flags = entity_flags_t::selected | entity_flags_t::active | entity_flags_t::hovered;
        selected_layer->drawer->fill(world_matrix, entity_flags_t::hovered, entity_flags_t::selected, entity_flags_t::active, draw_flags);
        gl::color hovered = 0x40000080;
        gl::color selected = 0x40008000;
        gl::color active = 0x40800000;
        blend_selection(hovered, selected, active, settings.multisamples);

        // Draw outline for active/hovered/selected entities in the selected layer
        if(settings.outline_width > 0.0f) {
            layer_render_target.bind_framebuffer();
            GL_CHECK(glViewport(0, 0, viewport_width, viewport_height));
            GL_CHECK(glClearColor(0, 0, 0, 0));
            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
            selected_layer->drawer->outline(settings.outline_width, world_matrix, viewport_size);
            blend_selection(gl::colors::red, gl::colors::green, gl::colors::blue, settings.multisamples);
        }
    }

    // draw the overlay graphics

    overlay.reset();

    if(mouse_mode == mouse_drag_zoom_select) {
        rect drag_rect_corrected = correct_aspect_ratio(view_rect.aspect_ratio(), drag_rect, aspect_expand);
        overlay.add_rect(drag_rect_corrected, 0x80ffff00);
        overlay.add_rect(drag_rect, 0x800000ff);
        overlay.add_outline_rect(drag_rect, 0xffffffff);
    }

    vec2d origin = viewport_pos_from_world_pos({ 0, 0 });

    gl::color axes_color = gl::colors::cyan;
    gl::color extent_color = gl::colors::yellow;

    if(settings.show_axes) {
        overlay.lines();
        overlay.add_line({ 0, origin.y }, { viewport_size.x, origin.y }, axes_color);
        overlay.add_line({ origin.x, 0 }, { origin.x, viewport_size.y }, axes_color);
    }

    if(settings.show_extent && selected_layer != nullptr && selected_layer->is_valid()) {
        if(active_entity != nullptr) {
            rect s = viewport_rect_from_board_rect(active_entity->bounds);
            overlay.add_outline_rect(s, extent_color);
        } else {
            rect extent = selected_layer->extent();
            if(extent.width() != 0 && extent.height() != 0) {
                rect s{ viewport_pos_from_world_pos(extent.min_pos), viewport_pos_from_world_pos(extent.max_pos) };
                overlay.add_outline_rect(s, extent_color);
            }
        }
    }

    if(mouse_mode == mouse_drag_select) {
        rect f{ drag_mouse_start_pos, drag_mouse_cur_pos };
        uint32_t color = 0x40ff8020;
        if(f.min_pos.x > f.max_pos.x) {
            color = 0x4080ff20;
        }
        overlay.add_rect(f, color);
        overlay.add_outline_rect(f, 0xffffffff);
    }

    color_program.activate();

    GL_CHECK(glUniformMatrix4fv(color_program.u_transform, 1, false, ortho_screen_matrix.m));

    overlay.draw();

    last_frame_cpu_time = get_time() - t;

    if(zoom_anim) {
        glfwPostEmptyEvent();
    }
}
