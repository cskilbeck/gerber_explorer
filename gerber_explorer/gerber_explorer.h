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
        gerber_3d::gl_drawer layer{};
        bool visible{ true };
        bool invert{ false };
        int draw_mode{0};
        bool expanded{ false };
        bool selected{ false };
        int alpha{ 255 };
        std::string name;

        std::string filename() const
        {
            if(is_valid()) {
                return layer.gerber_file->filename;
            }
            return {};
        }

        gl::colorf4 fill_color;
        gl::colorf4 clear_color;

        bool is_valid() const
        {
            return layer.gerber_file != nullptr;
        }

        gerber_lib::rect extent() const
        {
            if(!is_valid()) {
                return rect{};
            }
            return layer.gerber_file->image.info.extent;
        }

        bool operator<(gerber_layer const &other)
        {
            return index < other.index;
        }
    };

    using rect = gerber_lib::rect;
    using vec2d = gerber_lib::vec2d;
    using matrix = gerber_lib::matrix;

    enum mouse_drag_action
    {
        mouse_drag_none = 0,
        mouse_drag_pan,
        mouse_drag_zoom,
        mouse_drag_zoom_select,
        mouse_drag_maybe_select,
        mouse_drag_select
    };

    void set_mouse_mode(mouse_drag_action action);

    mouse_drag_action mouse_mode{};
    vec2d drag_mouse_cur_pos{};
    vec2d drag_mouse_start_pos{};
    vec2d mouse_world_pos{};
    rect drag_rect{};
    int ignore_mouse_moves{};

    // view rect in world coordinates
    rect view_rect{};

    // window screen coordinates
    vec2d window_size{};
    int window_xpos{};
    int window_ypos{};
    int window_width{};
    int window_height{};

    // viewport - area of the window left over after ImGui docking (in window coordinates)
    int viewport_xpos{};
    int viewport_ypos{};
    int viewport_width{};
    int viewport_height{};
    vec2d viewport_size{};
    rect viewport_rect{};

    // transform matrices
    gerber_3d::gl_matrix world_matrix{};
    gerber_3d::gl_matrix ortho_screen_matrix{};

    // gerber layers
    std::list<gerber_layer *> layers;     // active
    gerber_layer *selected_layer{ nullptr };

    // zoom to rect admin
    rect target_view_rect{};
    rect source_view_rect{};
    bool zoom_anim{ false };
    std::chrono::time_point<std::chrono::high_resolution_clock> target_view_time{};

    // overlay graphics (selection rectangle etc)
    gerber_3d::gl_drawlist overlay;

    // mouse moves are handled in the render function
    bool mouse_did_move{false};
    vec2d mouse_pos{};

    // glsl programs
    gerber_3d::gl_solid_program solid_program{};
    gerber_3d::gl_color_program color_program{};
    gerber_3d::gl_layer_program layer_program{};
    gerber_3d::gl_textured_program textured_program{};
    gerber_3d::gl_arc_program arc_program{};
    gerber_3d::gl_line2_program line2_program{};

    // for drawing things offscreen and then blending them to the final composition
    gerber_3d::gl_render_target layer_render_target{};

    int multisample_count{ 16 };
    int max_multisamples{ 1 };

    // when window size changes, zoom to fit (cleared if they pan/zoom etc manually)
    bool should_fit_to_viewport{ false };

    // bounding rect of all layers
    rect board_extent;
    vec2d board_center;

    // active entity admin
    std::vector<int> active_entities;
    int active_entity_index;
    gerber_3d::tesselator_entity const *active_entity{nullptr};
    std::string active_entity_description{};

    // how many gerbers queued up for loading?
    // when this transitions to 0, zoom to fit
    int gerbers_to_load{0};

    settings_t settings;

    std::mutex loader_mutex;
    std::mutex loaded_mutex;
    std::list<settings::layer_t> gerber_filenames_to_load;
    std::jthread gerber_load_thread;
    std::counting_semaphore<1024> loader_semaphore{0};

    void set_active_entity(gerber_3d::tesselator_entity const *entity);

    void update_board_extent();

    // -1 or 1 for each x,y based on settings.flip_x/y
    vec2d flip_xy;

    // scale window to view rect
    vec2d view_scale;

    vec2d world_pos_from_viewport_pos(vec2d const &p) const;
    rect world_rect_from_viewport_rect(rect const &r) const;

    vec2d viewport_pos_from_world_pos(vec2d const &p) const;
    rect viewport_rect_from_world_rect(rect const &r) const;
    rect viewport_rect_from_board_rect(rect const &r) const;
    vec2d viewport_pos_from_board_pos(vec2d const &p) const;

    vec2d board_pos_from_world_pos(vec2d const &p) const;
    vec2d board_pos_from_viewport_pos(vec2d const &p) const;
    rect board_rect_from_viewport_rect(rect const &r) const;
    rect board_rect_from_world_rect(rect const &r) const;

    void select_layer(gerber_layer *layer);

    void fit_to_viewport();
    void zoom_to_rect(rect const &zoom_rect, double border_ratio = 1.1);
    void zoom_at_point(vec2d const &zoom_pos, double zoom_scale);
    void update_view_rect();

    void blend_layer(gl::colorf4 const &col_r, gl::colorf4 const &col_g, gl::colorf4 const &col_b, float alpha);

    void load_gerber(settings::layer_t const &layer);

    std::list<gerber_layer *> loaded_layers; // loaded in the other thread, waiting to be added to layers

    void file_open();

    void load_gerbers(std::stop_token const &st);

    std::optional<std::filesystem::path> save_file_dialog();
    std::optional<std::filesystem::path> load_file_dialog();

    void save_settings(std::filesystem::path const &path);
    void load_settings(std::filesystem::path const &path);

    void on_window_size(int w, int h) override;
    void on_window_refresh() override;

    bool on_init() override;
    void on_render() override;
    void on_closed() override;
    void on_key(int key, int scancode, int action, int mods) override;
    void on_scroll(double xoffset, double yoffset) override;
    void on_mouse_button(int button, int action, int mods) override;
    void on_mouse_move(double xpos, double ypos) override;
    void on_drop(int count, const char **paths) override;

    void handle_mouse();

    void ui();

    std::string window_name() const override;
};
