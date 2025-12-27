#include <filesystem>

#include "gerber_explorer.h"
#include "gl_matrix.h"
#include "gl_colors.h"

LOG_CONTEXT("gerber_explorer", debug);

namespace
{
    using gerber_lib::gerber_2d::rect;
    using gerber_lib::gerber_2d::vec2d;
    using namespace gerber_3d;

    long long const zoom_lerp_time_ms = 700;

    double const drag_select_offset_start_distance = 16;

    uint32_t layer_colors[] = { gl_color::yellow,           gl_color::green, gl_color::dark_cyan, gl_color::lime_green, gl_color::antique_white,
                                gl_color::corn_flower_blue, gl_color::gold };

    uint32_t layer_color = gl_color::red;

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

    void make_world_to_window_transform(gl_matrix result, rect const &window, rect const &view)
    {
        gl_matrix scale;
        gl_matrix origin;

        make_scale(scale, (float)(window.width() / view.width()), (float)(window.height() / view.height()));
        make_translate(origin, -(float)view.min_pos.x, -(float)view.min_pos.y);
        matrix_multiply(scale, origin, result);
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::world_pos_from_window_pos(vec2d const &p) const
{
    vec2d scale = view_rect.size().divide(window_size);
    return vec2d{ p.x, window_size.y - p.y }.multiply(scale).add(view_rect.min_pos);
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::window_pos_from_world_pos(vec2d const &p) const
{
    vec2d scale = window_size.divide(view_rect.size());
    vec2d pos = p.subtract(view_rect.min_pos).multiply(scale);
    return { pos.x, window_size.y - pos.y };
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::fit_to_window()
{
    if(selected_layer != nullptr) {
        zoom_to_rect(selected_layer->layer->gerber_file->image.info.extent);
    } else {
        rect all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
        for(auto layer : layers) {
            all = all.union_with(layer->layer->gerber_file->image.info.extent);
        }
        zoom_to_rect(all);
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::zoom_to_rect(rect const &zoom_rect, double border_ratio)
{
    if(window_rect.width() == 0 || window_rect.height() == 0) {
        return;
    }
    rect new_rect = correct_aspect_ratio(window_rect.aspect_ratio(), zoom_rect, aspect_expand);
    vec2d mid = new_rect.mid_point();
    vec2d siz = new_rect.size().scale(border_ratio / 2);
    target_view_rect = { mid.subtract(siz), mid.add(siz) };
    source_view_rect = view_rect;
    target_view_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(zoom_lerp_time_ms);
    zoom_anim = true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::zoom_image(vec2d const &pos, double zoom_scale)
{
    // normalized position within view_rect
    vec2d zoom_pos = vec2d{ (double)pos.x, window_size.y - (double)pos.y }.divide(window_size);

    // scaled view_rect size
    vec2d new_size{ view_rect.width() / zoom_scale, view_rect.height() / zoom_scale };

    // world position of pos
    vec2d p = view_rect.min_pos.add(zoom_pos.multiply(view_rect.size()));

    // new rectangle offset from world position
    vec2d bottom_left = p.subtract(new_size.multiply(zoom_pos));
    vec2d top_right = bottom_left.add(new_size);
    view_rect = { bottom_left, top_right };
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
        vec2d wv = window_pos_from_world_pos(view_rect.min_pos);
        vec2d tv = window_pos_from_world_pos(target_view_rect.min_pos);
        if(wv.subtract(tv).length() <= 1) {
            view_rect = target_view_rect;
            zoom_anim = false;
        }
    }
}

//////////////////////////////////////////////////////////////////////

bool gerber_explorer::on_init()
{
    int window_width;
    int window_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    window_size.x = window_width;
    window_size.y = window_height;
    window_rect = { { 0, 0 }, window_size };
    view_rect = window_rect.offset(window_size.scale(-0.5));

    solid.init();
    solid.set_color(0xff00ffff);

    gerber_lib::gerber *g = new gerber_lib::gerber();
    g->parse_file("../../gerber_test_files/SMD_prim_20_X1.gbr");

    gerber_layer *layer = new gerber_layer();
    layer->layer = new gl_drawer();
    layer->layer->set_gerber(g);
    layer->layer->program = &solid;
    layer->layer->on_finished_loading();
    layer->fill_color = layer_colors[layers.size() % gerber_util::array_length(layer_colors)];
    layer->clear_color = gl_color::cyan;
    layer->outline_color = gl_color::magenta;
    layer->outline = true;
    layer->filename = std::filesystem::path(g->filename).filename().string();
    layers.push_back(layer);
    fit_to_window();
    return true;
}

//////////////////////////////////////////////////////////////////////

bool gerber_explorer::on_update()
{
    int window_width;
    int window_height;
    vec2d new_window_size;
    glfwGetWindowSize(window, &window_width, &window_height);
    new_window_size.x = window_width;
    new_window_size.y = window_height;
    if(new_window_size.x != window_size.x || new_window_size.y != window_size.y) {
        vec2d scale_factor = new_window_size.divide(window_size);
        window_size = new_window_size;
        window_rect = { { 0, 0 }, window_size };
        vec2d new_view_size = view_rect.size().multiply(scale_factor);
        view_rect.max_pos = view_rect.min_pos.add(new_view_size);
    }

    update_view_rect();

    gl_matrix projection_matrix_invert_y;
    gl_matrix projection_matrix;
    gl_matrix view_matrix;
    gl_matrix screen_matrix;

    // make a 1:1 screen matrix with origin in top left

    make_ortho(projection_matrix_invert_y, window_width, -window_height);
    make_translate(view_matrix, 0, (float)-window_size.y);
    matrix_multiply(projection_matrix_invert_y, view_matrix, screen_matrix);

    // make world to window matrix with origin in bottom left

    make_ortho(projection_matrix, window_width, window_height);
    make_world_to_window_transform(view_matrix, window_rect, view_rect);
    matrix_multiply(projection_matrix, view_matrix, world_transform_matrix);

    return true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_render()
{
    GL_CHECK(glViewport(0, 0, window_size.x, window_size.y));
    GL_CHECK(glClearColor(0.1f, 0.2f, 0.3f, 1.0f));
    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glDisable(GL_CULL_FACE));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    for(size_t n = layers.size(); n != 0;) {
        gerber_layer *layer = layers[--n];

        if(!layer->hide) {
            solid.use();
            GL_CHECK(glUniformMatrix4fv(solid.transform_location, 1, true, world_transform_matrix));

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            layer->draw(false, 1.0f);
        }
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_key(int key, int scancode, int action, int mods)
{
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_scroll(double xoffset, double yoffset)
{
    double scale_factor = (yoffset > 0) ? 1.1 : 0.9;
    zoom_image(mouse_pos, scale_factor);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_button(int button, int action, int mods)
{
    switch(action) {
    case GLFW_PRESS:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) {
                set_mouse_mode(mouse_drag_zoom_select, mouse_pos);
            } else {
                set_mouse_mode(mouse_drag_maybe_select, mouse_pos);
            }
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_pan, mouse_pos);
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_zoom, mouse_pos);
            break;
        }
        break;
    case GLFW_RELEASE:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if(mouse_mode == mouse_drag_zoom_select) {
                rect drag_rect_corrected = correct_aspect_ratio(window_rect.aspect_ratio(), drag_rect, aspect_expand);
                vec2d mn = drag_rect_corrected.min_pos;
                vec2d mx = drag_rect_corrected.max_pos;
                rect d = rect{ mn, mx }.normalize();
                if(d.width() > 2 && d.height() > 2) {
                    zoom_to_rect({ world_pos_from_window_pos(vec2d{ mn.x, mx.y }), world_pos_from_window_pos(vec2d{ mx.x, mn.y }) });
                }
            }
            set_mouse_mode(mouse_drag_none, {});
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_none, {});
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_none, {});
            break;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_move(double xpos, double ypos)
{
    mouse_pos = { xpos, ypos };

    switch(mouse_mode) {

    case mouse_drag_pan: {
        vec2d new_mouse_pos = world_pos_from_window_pos(mouse_pos);
        vec2d old_mouse_pos = world_pos_from_window_pos(drag_mouse_start_pos);
        view_rect = view_rect.offset(new_mouse_pos.subtract(old_mouse_pos).negate());
        drag_mouse_start_pos = mouse_pos;
    } break;

    case mouse_drag_zoom: {
        vec2d d = mouse_pos.subtract(drag_mouse_start_pos);
        zoom_image(drag_mouse_start_pos, 1.0 + (d.x - d.y) * 0.01);
        drag_mouse_cur_pos = mouse_pos;
        glfwSetCursorPos(window, drag_mouse_start_pos.x, drag_mouse_start_pos.y);
    } break;

    case mouse_drag_zoom_select: {
        drag_mouse_cur_pos = mouse_pos;
        if(drag_mouse_cur_pos.subtract(drag_mouse_start_pos).length() > 4) {
            drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos }.normalize();
        }
    } break;

    case mouse_drag_maybe_select: {
        if(mouse_pos.subtract(drag_mouse_start_pos).length() > drag_select_offset_start_distance) {
            set_mouse_mode(mouse_drag_select, mouse_pos);
            mouse_world_pos = world_pos_from_window_pos(mouse_pos);
        }
    } break;

    case mouse_drag_select: {
        drag_mouse_cur_pos = mouse_pos;
        drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
        // select_entities(drag_rect, (GetKeyState(VK_SHIFT) & 0x8000) == 0);
    } break;

    case mouse_drag_none: {
    } break;

    default:
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_closed()
{
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::set_mouse_mode(mouse_drag_action action, vec2d const &pos)
{
    auto show_mouse = [this] { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); };

    auto hide_mouse = [this] { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN); };

    auto begin = [&]() {
        zoom_anim = false;
        show_mouse();
        // SetCapture(hwnd);
    };

    switch(action) {

    case mouse_drag_none:
        zoom_anim = mouse_mode == mouse_drag_zoom_select;
        show_mouse();
        // ReleaseCapture();
        break;

    case mouse_drag_pan:
        drag_mouse_start_pos = pos;
        begin();
        break;

    case mouse_drag_zoom:
        zoom_anim = false;
        hide_mouse();
        // SetCapture(hwnd);
        drag_mouse_start_pos = pos;
        break;

    case mouse_drag_zoom_select:
        begin();
        drag_mouse_start_pos = pos;
        drag_rect = {};
        break;

    case mouse_drag_maybe_select: {
        begin();
        drag_mouse_start_pos = pos;
        vec2d world_pos = world_pos_from_window_pos(pos);
        // for(auto const &l : layers) {
        //     l->layer->tesselator.pick_entities(world_pos, l->selected_entities);
        // }
    } break;

    case mouse_drag_select:
        begin();
        drag_mouse_cur_pos = pos;
        drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
        break;
    }
    mouse_mode = action;
}
