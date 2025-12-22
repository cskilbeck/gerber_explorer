#pragma once

#include <string>
#include <format>

#include "gerber_2d.h"
#include "gerber_enums.h"
#include "gerber_util.h"

namespace gerber_lib
{
    struct gerber_image;

    //////////////////////////////////////////////////////////////////////

    struct gerber_knockout
    {
        bool first_instance{ false };
        gerber_knockout_type knockout_type{ knockout_type_no_knockout };
        gerber_polarity polarity{ polarity_positive };
        gerber_2d::vec2d lower_left{};
        gerber_2d::vec2d size{};
        double border{ 0.0 };

        std::string to_string() const
        {
            return std::format("KNOCKOUT: FIRST_INSTANCE: {}, TYPE: {}, POLARITY: {}, LOWER_LEFT: {}, SIZE: {}, BORDER: {}", first_instance,
                               knockout_type, polarity, lower_left.to_string(), size.to_string(), border);
        }

        gerber_knockout() = default;
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_step_and_repeat
    {
        gerber_2d::vec2d pos{ 1, 1 };
        gerber_2d::vec2d distance{ 0, 0 };

        std::string to_string() const
        {
            return std::format("STEP_AND_REPEAT: POS: {}, DISTANCE: {}", pos.to_string(), distance.to_string());
        }

        gerber_step_and_repeat() = default;
    };

    //////////////////////////////////////////////////////////////////////
    // gerber_level is to do with knockouts and alternating polarity etc

    struct gerber_level
    {
        gerber_knockout knockout{};
        gerber_step_and_repeat step_and_repeat{};
        gerber_polarity polarity{};
        std::string name;

        std::string to_string() const
        {
            return std::format("GERBER_LEVEL: KNOCKOUT: {}, STEP_AND_REPEAT: {}, POLARITY: {}, NAME: {}", knockout.to_string(), step_and_repeat.to_string(),
                               polarity, name);
        }

        gerber_level() = default;

        gerber_level(gerber_image *image);
    };

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_knockout);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_step_and_repeat);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_level);
