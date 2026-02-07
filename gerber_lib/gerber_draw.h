#pragma once

#include "gerber_2d.h"
#include "gerber_enums.h"

//////////////////////////////////////////////////////////////////////
// units are mm
// origin is lower left

namespace gerber_lib
{
    struct gerber_file;

    //////////////////////////////////////////////////////////////////////

    enum gerber_hide_elements
    {
        hide_element_none = 0,
        hide_element_lines = 1,
        hide_element_arcs = 2,
        hide_element_circles = 4,
        hide_element_rectangles = 8,
        hide_element_ovals = 16,
        hide_element_polygons = 32,
        hide_element_outlines = 64,
        hide_element_macros = 128
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_draw_element_type
    {
        draw_element_line = 0,
        draw_element_arc = 1
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_draw_element
    {
        gerber_draw_element_type draw_element_type;

        union
        {
            struct
            {
                vec2d start;
                vec2d end;
            } line;

            struct
            {
                vec2d center;
                double start_degrees;
                double end_degrees;
                double radius;
            } arc;
        };

        // ReSharper disable once CppPossiblyUninitializedMember
        gerber_draw_element()
        {
        }

        explicit gerber_draw_element(vec2d const &start, vec2d const &end) : draw_element_type(draw_element_line), line{ start, end }
        {
        }

        explicit gerber_draw_element(vec2d const &center, double start_degrees, double end_degrees, double radius)
            : draw_element_type(draw_element_arc), arc{ center, start_degrees, end_degrees, radius }
        {
        }

        std::string to_string() const
        {
            switch(draw_element_type) {
            case draw_element_line:
                return std::format("line: {},{}", line.start, line.end);
            case draw_element_arc:
                return std::format("arc: at {}, from {} to {}, radius {}", arc.center, arc.start_degrees, arc.end_degrees, arc.radius);
            }
            return std::string{ "huih?" };
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_draw_interface
    {
        virtual ~gerber_draw_interface() = default;
        virtual void set_gerber(gerber_file *g) = 0;

        // draw a filled shape of lines/arcs
        [[nodiscard]] virtual gerber_error_code fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, gerber_net *net) = 0;

        bool show_progress{ false };
    };

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_draw_element);
