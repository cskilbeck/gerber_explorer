#pragma once

// An entity might be one of:
// outline
// linear track
// arc
// flashed aperture

namespace gerber_lib
{
    struct gerber_net;

    struct gerber_entity
    {
        int line_number_begin;
        int line_number_end;
        size_t net_index;
        std::map<std::string, std::string> attributes;

        gerber_entity(int begin, int end, size_t net_id) : line_number_begin(begin), line_number_end(end), net_index(net_id)
        {
        }

        std::string to_string() const
        {
            return std::format("NET INDEX {}, LINE BEGIN {}, LINE END {}", net_index, line_number_begin, line_number_end);
        }
    };

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_entity);