//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cfloat>

#include "gerber_2d.h"
#include "gerber_enums.h"
#include "gerber_stats.h"
#include "gerber_format.h"

namespace gerber_lib
{
    struct gerber_file;
    struct gerber_aperture;
    struct gerber_aperture_macro;
    struct gerber_net;
    struct gerber_level;
    struct gerber_net_state;

    //////////////////////////////////////////////////////////////////////

    struct gerber_image_info
    {
        std::string image_name;
        gerber_polarity polarity{ polarity_unspecified };
        rect extent{ DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX };
        double offset_a{};
        double offset_b{};
        double image_rotation{};
        gerber_image_justify justify_a{ image_justify_none };
        gerber_image_justify justify_b{ image_justify_none };
        double image_justify_offset_a{};
        double image_justify_offset_b{};
        double image_justify_offset_actual_a{};
        double image_justify_offset_actual_b{};
        std::string plotter_film;
        std::string filetype_name;

        gerber_image_info() = default;
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber;

    struct gerber_image
    {
        struct aperture_translate
        {
            int source_index{};
            int new_index{};
        };

        gerber_file_type file_type{ file_type_rs274x };
        gerber_stats stats{};
        gerber_format format;
        std::map<int, gerber_aperture *> apertures;
        std::vector<gerber_aperture_macro *> aperture_macros;
        std::vector<gerber_net *> nets;
        std::vector<gerber_level *> levels;
        std::vector<gerber_net_state *> net_states;

        gerber_image_info info;
        gerber_file *gerber;

        gerber_image() = default;

        void cleanup();

        ~gerber_image();
    };

}    // namespace gerber_lib
