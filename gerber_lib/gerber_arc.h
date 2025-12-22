//////////////////////////////////////////////////////////////////////

#pragma once

#include <format>

#include "gerber_2d.h"
#include "gerber_util.h"

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    struct gerber_arc
    {
        gerber_2d::vec2d pos{};
        gerber_2d::vec2d size{};
        double start_angle{};
        double end_angle{};

        gerber_arc() = default;

        gerber_arc(double x, double y, double w, double h, double start, double end) : pos(x, y), size(w, h), start_angle(start), end_angle(end)
        {
        }

        std::string to_string() const
        {
            return std::format("ARC: POS: {}, SIZE: {}, START: {}, END: {}", pos, size, start_angle, end_angle);
        }

        double sweep_angle() const
        {
            double sweep = end_angle - start_angle;
            if(sweep == 0.0) {
                sweep = 360.0;
            }
            return sweep;
        }
    };

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_arc);
