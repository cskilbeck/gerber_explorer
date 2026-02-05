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

#include <expected>

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

//////////////////////////////////////////////////////////////////////

struct gerber_layer
{
    using rect = gerber_lib::rect;
    using vec2d = gerber_lib::vec2d;
    using matrix = gerber_lib::matrix;

    void init()
    {
        drawers[0].init(this);
        drawers[1].init(this);
    }

    // have two gl_drawer instances and a pointer to one of them
    // tesselate into the idle one and swap it over when that's complete (in the main thread)

    gerber::gl_drawer *drawer{};
    gerber::gl_drawer drawers[2]{};
    int current_drawer{ 0 };

    gerber_lib::gerber_file *file;

    int index;
    bool visible{ true };
    bool invert{ false };
    bool expanded{ false };
    bool selected{ false };
    bool is_outline_layer{ false };
    int alpha{ 255 };

    bool got_mask{false};

    std::string name;
    layer_order_t layer_order{ layer_order_t::all };
    gl::color fill_color;
    gl::color clear_color;

    gerber_lib::layer::type_t layer_type() const
    {
        if(file != nullptr) {
            return file->layer_type;
        }
        return gerber_lib::layer::type_t::unknown;
    }

    std::string filename() const
    {
        if(is_valid()) {
            return file->filename;
        }
        return {};
    }

    bool is_valid() const
    {
        return drawer != nullptr && file != nullptr;
    }

    gerber_lib::rect extent() const
    {
        if(!is_valid()) {
            return rect{};
        }
        return file->image.info.extent;
    }

    bool operator<(gerber_layer const &other)
    {
        return index < other.index;
    }
};


struct gerber_explorer : gl_window
{
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
        job_type_create_mask = 4,
    };

    gerber::tesselation_quality_t tesselate_quality{ gerber::tesselation_quality::medium };

    void set_mouse_mode(mouse_drag_action action);

    double last_frame_cpu_time{};

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
    gl::matrix world_matrix{};
    gl::matrix ortho_screen_matrix{};

    // gerber layers
    std::list<gerber_layer *> layers;    // active
    gerber_layer *selected_layer{ nullptr };
    bool isolate_selected_layer{ false };

    // zoom to rect admin
    rect target_view_rect{};
    rect source_view_rect{};
    bool zoom_anim{ false };
    std::chrono::time_point<std::chrono::high_resolution_clock> target_view_time{};

    // overlay graphics (selection rectangle etc)
    gl::drawlist overlay;

    // mouse moves are handled in the render function
    bool mouse_did_move{ false };
    vec2d mouse_pos{};
    vec2d prev_mouse_pos{};
    vec2d world_mouse_pos{};

    // glsl programs
    static gl::solid_program solid_program;
    static gl::color_program color_program;
    static gl::layer_program layer_program;
    static gl::blit_program blit_program;
    static gl::selection_program selection_program;
    static gl::arc_program arc_program;
    static gl::line2_program line2_program;

    // for drawing things offscreen and then blending them to the final composition
    gl::render_target layer_render_target{};

    int max_multisamples{ 1 };

    // when window size changes, zoom to fit (cleared if they pan/zoom etc manually)
    bool should_fit_to_viewport{ false };

    // bounding rect of all layers
    rect board_extent;
    vec2d board_center;

    // active entity admin
    std::vector<int> active_entities;
    int active_entity_index;
    gerber::tesselator_entity *active_entity{ nullptr };
    std::string active_entity_description{};

    job_pool pool;

    std::mutex layer_drawer_mutex;

    bool retesselate{ false };

    enum tesselation_options_t : int
    {
        tesselation_options_none = 0,
        tesselation_options_force_outline = 1,
    };

    void tesselate_layer(gerber_layer *layer, tesselation_options_t options = tesselation_options_none);

    std::list<gerber_layer *> loaded_layers;
    std::mutex loaded_mutex;

    settings_t settings;

    void set_active_entity(gerber::tesselator_entity *entity);

    void update_board_extent();

    // -1 or 1 for each x,y based on settings.flip_x/y
    vec2d flip_xy;

    // scale window to view rect
    vec2d view_scale;

    gerber_layer *get_outline_layer() const;
    void set_outline_layer(gerber_layer *new_outline_layer);

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

    void blend_layer(gl::color color_fill, gl::color color_other, int num_samples);
    void blend_selection(gl::color red, gl::color green, gl::color blue, int num_samples);

    void add_gerber(settings::layer_t const &layer);

    void load_gerber(settings::layer_t const &layer_to_load);

    void file_open();

    void close_all_layers();

    std::expected<std::filesystem::path, std::error_code> save_file_dialog();
    std::expected<std::filesystem::path, std::error_code> load_file_dialog();

    void save_settings(std::filesystem::path const &path, bool save_files);
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
