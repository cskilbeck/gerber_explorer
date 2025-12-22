//////////////////////////////////////////////////////////////////////
// OpenGL renderer for gerber explorer

#pragma once

#define WIN32_LEAN_AND_MEAN
// #define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_functions.h"
#include "gl_colors.h"

#include "gerber_lib.h"
#include "gerber_log.h"
#include "gerber_2d.h"
#include "gerber_draw.h"
#include "gl_base.h"

// #include "occ_drawer.h"

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    // struct gl_drawer;
    struct tesselator_entity;

    //////////////////////////////////////////////////////////////////////

    struct gl_window
    {
        LOG_CONTEXT("gl_window", info);

        // //////////////////////////////////////////////////////////////////////
        //
        // struct gerber_layer
        // {
        //     int index;
        //     gl_drawer *layer{ nullptr };
        //     bool hide{ false };
        //     bool outline{ false };
        //     bool fill{ true };
        //     bool expanded{ false };
        //     bool selected{ false };
        //     int alpha{ 255 };
        //     std::string filename;
        //     uint32_t fill_color;
        //     uint32_t clear_color;
        //     uint32_t outline_color;
        //
        //     std::list<tesselator_entity const *> selected_entities;
        //
        //     void draw(bool wireframe, float outline_thickness);
        //
        //     bool operator<(gerber_layer const &other)
        //     {
        //         return index < other.index;
        //     }
        // };
        //
        //////////////////////////////////////////////////////////////////////

        using vec2d = gerber_lib::gerber_2d::vec2d;
        using rect = gerber_lib::gerber_2d::rect;
        using matrix = gerber_lib::gerber_2d::matrix;

        int create_window(int x_pos, int y_pos, int client_width, int client_height);
        LRESULT CALLBACK wnd_proc(UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

        void fit_to_window();

        void zoom_to_rect(rect const &zoom_rect, double border_ratio = 1.1);
        void zoom_image(vec2d const &pos, double zoom_scale);
        vec2d window_pos_from_world_pos(vec2d const &p) const;
        vec2d world_pos_from_window_pos(vec2d const &p) const;

        void load_gerber_files(std::vector<std::string> filenames);

        void update_view_rect();
        void draw(int width_pixels, int height_pixels);

        void render();

        void ui(int wide, int high);

        // void select_layer(gerber_layer *l);
        //
        void cleanup()
        {
        }

        bool get_open_filenames(std::vector<std::string> &filenames);

        enum mouse_drag_action
        {
            mouse_drag_none = 0,
            mouse_drag_pan,
            mouse_drag_zoom,
            mouse_drag_zoom_select,
            mouse_drag_maybe_select,
            mouse_drag_select
        };

        void set_mouse_mode(mouse_drag_action action, vec2d const &pos);
        void on_mouse_move(vec2d const &mouse_pos);

        // gerber_layer *selected_layer{ nullptr };
        //
        // std::vector<gerber_layer *> layers;
        //
        mouse_drag_action mouse_mode{};

        vec2d drag_mouse_cur_pos{};
        vec2d drag_mouse_start_pos{};

        vec2d mouse_world_pos{};
        bool imperial{};

        rect drag_rect{};

        bool show_options{ false };
        bool show_stats{ false };

        bool show_axes{ true };
        bool show_extent{ true };

        bool wireframe{ false };

        int multisample_count{ 4 };
        int max_multisamples{ 1 };

        float outline_thickness{ 3 };

        bool window_size_valid{ false };

        bool flip_x{ false };
        bool flip_y{ false };

        uint32_t axes_color{ 0x60ffffff };
        uint32_t extent_color{ 0xC000ffff };
        uint32_t background_color{ 0xff602010 };

        gl_solid_program solid_program{};
        gl_layer_program layer_program{};
        gl_color_program color_program{};
        gl_textured_program textured_program{};

        gl_drawlist overlay{};

        gl_render_target my_target{};

        gl_vertex_array_textured fullscreen_blit_verts;

        int window_width{};
        int window_height{};

        vec2d window_size{};
        rect window_rect{};
        rect view_rect{};
        rect target_view_rect{};
        rect source_view_rect{};
        bool zoom_anim{ false };

        std::chrono::time_point<std::chrono::high_resolution_clock> target_view_time;

        HGLRC render_context{};
        HWND hwnd{};
        HDC window_dc{};
        RECT normal_rect;
        bool fullscreen{};
        bool quit{};

        void show_3d_view();
    };

}    // namespace gerber_3d
