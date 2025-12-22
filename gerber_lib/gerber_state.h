#pragma once

#include "gerber_enums.h"

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    struct gerber_net;
    struct gerber_level;
    struct gerber_net_state;

    struct gerber_state
    {
        int current_x{};
        int current_y{};

        int previous_x{};
        int previous_y{};

        int center_x{};
        int center_y{};

        int current_aperture{};

        bool changed_state{ false };

        gerber_aperture_state aperture_state;
        gerber_interpolation interpolation;
        gerber_interpolation previous_interpolation;

        bool is_region_fill{ false };
        bool is_multi_quadrant{ false };

        // these are borrowed
        gerber_net *region_start_node{ nullptr };
        gerber_level *level{ nullptr };
        gerber_net_state *net_state{ nullptr }; 

        gerber_state() = default;
    };
}    // namespace gerber_lib
