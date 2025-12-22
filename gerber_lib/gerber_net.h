//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <format>

#include "gerber_2d.h"
#include "gerber_arc.h"
#include "gerber_enums.h"
#include "gerber_util.h"

namespace gerber_lib
{
    struct gerber_image;
    struct gerber_level;

    //////////////////////////////////////////////////////////////////////

    struct gerber_net_state
    {
        gerber_axis_select axis_select{ axis_select_none };
        gerber_mirror_state mirror_state{ mirror_state_none };
        gerber_unit unit{ unit_inch };
        gerber_2d::vec2d offset{ 0.0, 0.0 };
        gerber_2d::vec2d scale{ 1.0, 1.0 };

        std::string to_string() const;

        gerber_net_state() = default;

        gerber_net_state(gerber_image *img);
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_net
    {
        gerber_2d::vec2d start{};
        gerber_2d::vec2d end{};
        gerber_2d::rect bounding_box{};
        int aperture{};
        gerber_aperture_state aperture_state{ aperture_state_off };
        gerber_interpolation interpolation_method{ interpolation_linear };
        gerber_arc circle_segment{};
        int num_region_points{};
        std::string label;
        bool hidden{ false };
        int entity_id{ 0 };

        // these are borrowed...
        gerber_level *level{ nullptr };
        gerber_net_state *net_state{ nullptr };

        std::string to_string() const;

        gerber_net() = default;

        gerber_net(gerber_image *img);

        gerber_net(gerber_image *img, gerber_net *cur_net, gerber_level *lvl, gerber_net_state *state);
    };

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_net_state);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_net);
