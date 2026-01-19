//////////////////////////////////////////////////////////////////////
// Experiment:
// Use C++ 20 but
// Don't really use OOP much
// Don't give a shit about CPU performance
// Don't give a shit about memory efficiency
// Don't give a shit about compile times
// Don't give a shit about leaks
// Make the API minimal
// Make it conformant

#pragma once

#include "gerber_error.h"
#include "gerber_stats.h"
#include "gerber_image.h"
#include "gerber_state.h"
#include "gerber_level.h"
#include "gerber_entity.h"
#include "gerber_reader.h"
#include "gerber_draw.h"

namespace gerber_lib
{
    namespace layer
    {
        enum type_t
        {
            unknown = 0,
            vcut = 1000,
            board = 1001,
            outline = 1002,
            mechanical = 1003,
            info = 1004,
            keepout = 1005,

            drill = 2000,
            paste_top = 3000,
            overlay_top = 4000,
            soldermask_top = 5000,
            drill_top = 5500,
            copper_top = 6000,
            copper_inner = 7000,
            copper_bottom = 8000,
            drill_bottom = 8500,
            soldermask_bottom = 9000,
            overlay_bottom = 10000,
            paste_bottom = 11000,
        };
    }

    struct gerber_net;
    struct gerber_macro_parameters;

    //////////////////////////////////////////////////////////////////////

    struct gerber
    {
        static constexpr int min_aperture = 10;
        static constexpr int max_num_apertures = 9999;

        std::string filename;
        layer::type_t layer_type;

        double image_scale_a{ 1.0 };
        double image_scale_b{ 1.0 };
        double image_rotation{ 0.0 };

        matrix aperture_matrix;

        bool knockout_measure{ false };
        vec2d knockout_limit_min{};
        vec2d knockout_limit_max{};

        gerber_level knockout_level{};

        int accuracy_decimal_places{ 6 };

        int current_net_id{};

        gerber_stats stats{};
        gerber_image image{};
        gerber_state state{};
        gerber_reader reader{};

        std::map<std::string, std::string> attributes;
        std::vector<std::string> comments;

        std::vector<gerber_entity> entities;

        gerber_entity &add_entity();

        void reset();

        void cleanup();

        void update_net_bounds(rect &bounds, std::vector<vec2d> const &points) const;
        void update_net_bounds(rect &bounds, double x, double y, double w, double h) const;
        void update_image_bounds(rect &bounds, double repeat_offset_x, double repeat_offset_y, gerber_image &cur_image) const;

        gerber_error_code get_aperture_points(gerber_macro_parameters const &macro, gerber_net *net, std::vector<vec2d> &points) const;

        gerber_error_code parse_file(char const *file_path);
        gerber_error_code parse_memory(char const *data, size_t size);

        gerber_error_code do_parse();

        gerber_error_code draw(gerber_draw_interface &drawer) const;
        gerber_error_code lines(gerber_draw_interface &drawer) const;
        gerber_error_code fill_region_path(gerber_draw_interface &drawer, size_t net_index, gerber_polarity polarity) const;

        gerber_error_code draw_linear_interpolation(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const;
        gerber_error_code draw_linear_circle(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const;
        gerber_error_code draw_linear_rectangle(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const;
        gerber_error_code draw_macro(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *macro_aperture) const;
        gerber_error_code draw_capsule(gerber_draw_interface &drawer, gerber_net *net, double width, double height) const;
        gerber_error_code draw_arc(gerber_draw_interface &drawer, gerber_net *net, double thickness) const;
        gerber_error_code draw_circle(gerber_draw_interface &drawer, gerber_net *net, vec2d const &pos, double radius) const;
        gerber_error_code draw_rectangle(gerber_draw_interface &drawer, gerber_net *net, rect const &draw_rect) const;

        gerber_error_code fill_polygon(gerber_draw_interface &drawer, double diameter, int num_sides, double angle_degrees) const;

        gerber_error_code parse_gerber_segment(gerber_net *net);

        gerber_error_code parse_g_code();
        gerber_error_code parse_d_code();
        gerber_error_code parse_tf_code();
        bool parse_m_code();

        gerber_error_code parse_rs274x(gerber_net *net);

        layer::type_t classify() const;

        gerber_error_code parse_aperture_definition(gerber_aperture *aperture, gerber_image *cur_image, double unit_scale);

        void update_knockout_measurements();

        gerber() = default;

        ~gerber() = default;
    };

}    // namespace gerber_lib
