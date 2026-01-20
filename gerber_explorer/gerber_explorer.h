#pragma once

#include <thread>
#include <mutex>

#include "gl_window.h"
#include "gl_base.h"
#include "gl_colors.h"
#include "gl_matrix.h"
#include "gl_drawer.h"

#include "job_pool.h"

#include "settings.h"

enum class layer_order_t
{
    all,
    drill,    // drill is special, should be between top_outer and top_copper OR bottom_outer and botttom_copper (see drill_top, drill_bottom)
    top_outer,
    drill_top,
    top_copper,
    inner_copper,
    bottom_copper,
    drill_bottom,
    bottom_outer,
    other
};

struct gerber_explorer : gl_window
{

    //////////////////////////////////////////////////////////////////////

    struct gerber_layer
    {
        int index;

        // have two gl_drawer instances and a pointer to one of them
        // tesselate into the idle one and swap it over when that's complete

        gerber_3d::gl_drawer *drawer;

        gerber_3d::gl_drawer drawers[2];

        // need a way to atomically swap
        int current_drawer{ 0 };

        bool visible{ true };
        bool invert{ false };
        bool expanded{ false };
        bool selected{ false };
        int alpha{ 255 };
        std::string name;
        layer_order_t layer_order;
        gerber_lib::layer::type_t layer_type;

        std::string filename() const
        {
            if(is_valid()) {
                return drawer->gerber_file->filename;
            }
            return {};
        }

        gl::color fill_color;
        gl::color clear_color;

        bool is_valid() const
        {
            return drawer != nullptr && drawer->gerber_file != nullptr;
        }

        gerber_lib::rect extent() const
        {
            if(!is_valid()) {
                return rect{};
            }
            return drawer->gerber_file->image.info.extent;
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

    enum job_type_t
    {
        job_type_load_gerber = 1,
        job_type_tesselate = 2,
    };

    gerber_3d::tesselation_quality_t tesselate_quality{ gerber_3d::tesselation_quality::medium };

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
    std::list<gerber_layer *> layers;    // active
    gerber_layer *selected_layer{ nullptr };
    bool isolate_selected_layer{false};

    // zoom to rect admin
    rect target_view_rect{};
    rect source_view_rect{};
    bool zoom_anim{ false };
    std::chrono::time_point<std::chrono::high_resolution_clock> target_view_time{};

    // overlay graphics (selection rectangle etc)
    gerber_3d::gl_drawlist overlay;

    // mouse moves are handled in the render function
    bool mouse_did_move{ false };
    vec2d mouse_pos{};
    vec2d prev_mouse_pos{};

    // glsl programs
    static gerber_3d::gl_solid_program solid_program;
    static gerber_3d::gl_color_program color_program;
    static gerber_3d::gl_layer_program layer_program;
    static gerber_3d::gl_textured_program textured_program;
    static gerber_3d::gl_arc_program arc_program;
    static gerber_3d::gl_line2_program line2_program;

    // for drawing things offscreen and then blending them to the final composition
    gerber_3d::gl_render_target layer_render_target{};

    int max_multisamples{ 1 };

    // when window size changes, zoom to fit (cleared if they pan/zoom etc manually)
    bool should_fit_to_viewport{ false };

    // bounding rect of all layers
    rect board_extent;
    vec2d board_center;

    // active entity admin
    std::vector<int> active_entities;
    int active_entity_index;
    gerber_3d::tesselator_entity *active_entity{ nullptr };
    std::string active_entity_description{};

    job_pool pool;

    std::mutex layer_drawer_mutex;

    bool retesselate{false};

    void tesselate_layer(gerber_layer *layer)
    {
        pool.add_job(job_type_tesselate, [this, layer](std::stop_token st) {
            gerber_3d::gl_drawer *d = &layer->drawers[1 - layer->current_drawer];
            d->tesselation_quality = settings.tesselation_quality;
            d->set_gerber(layer->drawer->gerber_file);
            {
                std::lock_guard l(layer_drawer_mutex);
                layer->current_drawer = 1 - layer->current_drawer;
            }
        });
    }

    std::list<gerber_layer *> loaded_layers;
    std::mutex loaded_mutex;

    settings_t settings;

    void set_active_entity(gerber_3d::tesselator_entity *entity);

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

    bool layer_is_visible(gerber_layer const *layer) const;
    void next_view();

    void blend_layer(gl::color col_r, gl::color col_g, gl::color col_b, float alpha, int num_samples = 0);

    void add_gerber(settings::layer_t const &layer);

    void load_gerber(settings::layer_t const &layer_to_load);

    void file_open();

    void close_all_layers();

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

    bool is_idle() override;

    void handle_mouse();

    void ui();

    std::string window_name() const override;
};
