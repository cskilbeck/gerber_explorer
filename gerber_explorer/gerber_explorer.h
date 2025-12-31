#pragma once

#include <thread>
#include <mutex>
#include <semaphore>

#include "gl_window.h"
#include "gl_base.h"
#include "gl_colors.h"
#include "gl_matrix.h"

#include "settings.h"

struct gerber_explorer : gl_window {

    //////////////////////////////////////////////////////////////////////

    struct gerber_layer
    {
        int index;
        gerber_3d::gl_drawer *layer{ nullptr };
        bool show{ true };
        bool invert{ false };
        bool outline{ false };
        bool fill{ true };
        bool expanded{ false };
        bool selected{ false };
        int alpha{ 255 };
        std::string name;

        gl_color::float4 fill_color;
        gl_color::float4 clear_color;
        gl_color::float4 outline_color;

        bool is_valid() const
        {
            return layer != nullptr && layer->gerber_file != nullptr;
        }

        gerber_lib::gerber_2d::rect extent() const
        {
            if(!is_valid()) {
                return rect{};
            }
            return layer->gerber_file->image.info.extent;
        }

        void draw(bool wireframe, float outline_thickness)
        {
            layer->draw(fill, outline, wireframe, outline_thickness);
        }

        bool operator<(gerber_layer const &other)
        {
            return index < other.index;
        }
    };

    using rect = gerber_lib::gerber_2d::rect;
    using vec2d = gerber_lib::gerber_2d::vec2d;
    using matrix = gerber_lib::gerber_2d::matrix;

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

    mouse_drag_action mouse_mode{};
    vec2d drag_mouse_cur_pos{};
    vec2d drag_mouse_start_pos{};
    vec2d mouse_world_pos{};
    rect drag_rect{};

    // view rect in world coordinates
    rect view_rect{};

    // window rect in screen coordinates
    rect window_rect{};

    vec2d window_size{};

    int window_width{};
    int window_height{};

    int window_xpos{};
    int window_ypos{};

    gerber_layer *selected_layer{ nullptr };
    std::list<gerber_layer *> layers;     // active

    rect target_view_rect{};
    rect source_view_rect{};
    bool zoom_anim{ false };
    std::chrono::time_point<std::chrono::high_resolution_clock> target_view_time{};
    gerber_3d::gl_matrix world_matrix{};
    gerber_3d::gl_matrix screen_matrix{};
    gerber_3d::gl_matrix projection_matrix{};

    gerber_3d::gl_drawlist overlay;

    gerber_3d::gl_drawer drawer{};

    vec2d mouse_pos{};

    int debug_draw_call = 0;
    int debug_outline_line = 0;

    gerber_3d::gl_solid_program solid_program{};
    gerber_3d::gl_color_program color_program{};
    gerber_3d::gl_layer_program layer_program{};
    gerber_3d::gl_textured_program textured_program{};

    gerber_3d::gl_vertex_array_textured fullscreen_blit_verts;

    gerber_3d::gl_render_target my_target{};

    int multisample_count{ 4 };
    int max_multisamples{ 1 };

    vec2d world_pos_from_window_pos(vec2d const &p) const;
    vec2d window_pos_from_world_pos(vec2d const &p) const;
    void fit_to_window();
    void zoom_to_rect(rect const &zoom_rect, double border_ratio = 1.1);
    void zoom_image(vec2d const &pos, double zoom_scale);
    void update_view_rect();

    void load_gerber(char const *filename);

    void select_entity(vec2d const &window_pos);

    std::list<gerber_layer *> loaded_layers; // loaded in the other thread, waiting to be added to layers

    settings_t settings;

    std::mutex loader_mutex;
    std::mutex loaded_mutex;
    std::list<std::string> gerber_filenames_to_load;
    void load_gerbers(std::stop_token const &st);
    std::jthread gerber_load_thread;
    std::counting_semaphore<1024> loader_semaphore{0};

    bool on_init() override;
    void on_render() override;
    void on_closed() override;
    void on_key(int key, int scancode, int action, int mods) override;
    void on_scroll(double xoffset, double yoffset) override;
    void on_mouse_button(int button, int action, int mods) override;
    void on_mouse_move(double xpos, double ypos) override;

    std::string app_name() const override;
};
