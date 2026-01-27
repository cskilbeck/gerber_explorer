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

            other = 1000,
            vcut = 1001,
            board = 1002,
            outline = 1003,
            mechanical = 1004,
            info = 1005,
            keepout = 1006,

            pads = 2000,
            drill = 3000,

            paste_top = 4000,
            pads_top = 5000,
            overlay_top = 6000,
            soldermask_top = 7000,
            drill_top = 8000,
            copper_top = 9000,
            copper_inner = 10000,
            copper_bottom = 11000,
            drill_bottom = 12000,
            soldermask_bottom = 13000,
            overlay_bottom = 14000,
            pads_bottom = 15000,
            paste_bottom = 16000,
        };
    }

    //////////////////////////////////////////////////////////////////////

    inline bool is_layer_type(int l, layer::type_t t)
    {
        return l >= t && l < (t + 1000);
    }

    //////////////////////////////////////////////////////////////////////

    struct gerber_net;
    struct gerber_macro_parameters;

    struct gerber_file
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

        gerber_file() = default;

        ~gerber_file() = default;
    };

}    // namespace gerber_lib
