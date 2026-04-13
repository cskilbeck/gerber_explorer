//////////////////////////////////////////////////////////////////////

#include <cmath>
#include <cstring>
#include <charconv>
#include <string_view>

#include "gerber_lib.h"
#include "gerber_net.h"
#include "gerber_aperture.h"

LOG_CONTEXT("drill_parser", info);

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    bool gerber_file::detect_excellon() const
    {
        // Peek at the start of the file for M48 (Excellon header marker)
        size_t pos = 0;
        while(pos < reader.file_size) {
            char c = reader.file_data[pos];
            if(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\xEF' || c == '\xBB' || c == '\xBF') {
                pos += 1;
                continue;
            }
            break;
        }
        if(pos + 2 < reader.file_size) {
            return reader.file_data[pos] == 'M' && reader.file_data[pos + 1] == '4' && reader.file_data[pos + 2] == '8';
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////

    // Parse an integer from a string_view, advancing pos past the digits.
    // Returns 0 if no digits found.
    static int parse_int(std::string_view line, size_t &pos)
    {
        int value = 0;
        bool negative = false;
        if(pos < line.size() && (line[pos] == '-' || line[pos] == '+')) {
            negative = line[pos] == '-';
            pos += 1;
        }
        while(pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
            value = value * 10 + (line[pos] - '0');
            pos += 1;
        }
        return negative ? -value : value;
    }

    // Parse digits and return the raw string of digits (for zero-padding logic)
    static std::string_view parse_digits(std::string_view line, size_t &pos)
    {
        size_t start = pos;
        if(pos < line.size() && (line[pos] == '-' || line[pos] == '+')) {
            pos += 1;
        }
        while(pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
            pos += 1;
        }
        return line.substr(start, pos - start);
    }

    // Convert a coordinate digit string to mm, handling zero suppression and format
    static double convert_coordinate(std::string_view digits, int total_places, bool trailing_zero_suppression, double unit_scale, double decimal_divisor)
    {
        if(digits.empty()) {
            return 0.0;
        }

        bool negative = false;
        std::string_view raw = digits;
        if(raw[0] == '-' || raw[0] == '+') {
            negative = raw[0] == '-';
            raw = raw.substr(1);
        }

        int64_t value = 0;
        for(char c : raw) {
            if(c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
            }
        }

        // LZ: trailing zeros suppressed, so missing digits go on the right (multiply up)
        // TZ: leading zeros suppressed, so missing digits go on the left (already correct)
        if(!trailing_zero_suppression) {
            for(int i = static_cast<int>(raw.size()); i < total_places; ++i) {
                value *= 10;
            }
        }

        double result = static_cast<double>(value) / decimal_divisor * unit_scale;
        return negative ? -result : result;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_drill_file()
    {
        LOG_CONTEXT("parse_drill", info);

        image.file_type = file_type_drill;

        bool in_header = false;
        int current_tool = 0;
        double prev_x = 0.0;
        double prev_y = 0.0;
        int integer_places = 2;
        int decimal_places = 4;
        bool units_inch = true;
        bool trailing_zero_suppression = false;    // LZ mode by default (leading zeros present, trailing may be suppressed)
        bool format_explicit = false;

        double unit_scale = 25.4;
        double decimal_divisor = 10000.0;

        int entity_id = 0;
        gerber_net *prev_net = image.nets[0];

        auto update_derived = [&]() {
            unit_scale = units_inch ? 25.4 : 1.0;
            decimal_divisor = std::pow(10.0, decimal_places);
            if(!format_explicit) {
                if(units_inch) {
                    integer_places = 2;
                    decimal_places = 4;
                } else {
                    integer_places = 3;
                    decimal_places = 3;
                }
                decimal_divisor = std::pow(10.0, decimal_places);
            }
        };

        auto make_flash_net = [&](double x_mm, double y_mm) {
            add_entity();
            entity_id += 1;

            gerber_net *net = new gerber_net(&image, prev_net, state.level, state.net_state);
            net->entity_id = entity_id;
            net->start.x = x_mm;
            net->start.y = y_mm;
            net->end.x = x_mm;
            net->end.y = y_mm;
            net->aperture = current_tool;
            net->aperture_state = aperture_state_flash;
            net->interpolation_method = interpolation_linear;

            // Compute bounding box from tool diameter
            double radius = 0.0;
            auto it = image.apertures.find(current_tool);
            if(it != image.apertures.end()) {
                radius = it->second->parameters[0] / 2.0;
            }
            net->bounding_box = { x_mm - radius, y_mm - radius, x_mm + radius, y_mm + radius };
            update_image_bounds(net->bounding_box, 0, 0, image);

            prev_net = net;
        };

        auto make_slot_net = [&](double x1, double y1, double x2, double y2) {
            add_entity();
            entity_id += 1;

            gerber_net *net = new gerber_net(&image, prev_net, state.level, state.net_state);
            net->entity_id = entity_id;
            net->start.x = x1;
            net->start.y = y1;
            net->end.x = x2;
            net->end.y = y2;
            net->aperture = current_tool;
            net->aperture_state = aperture_state_on;
            net->interpolation_method = interpolation_linear;

            // Bounding box: capsule around the line
            double radius = 0.0;
            auto it = image.apertures.find(current_tool);
            if(it != image.apertures.end()) {
                radius = it->second->parameters[0] / 2.0;
            }
            double min_x = std::min(x1, x2) - radius;
            double min_y = std::min(y1, y2) - radius;
            double max_x = std::max(x1, x2) + radius;
            double max_y = std::max(y1, y2) + radius;
            net->bounding_box = { min_x, min_y, max_x, max_y };
            update_image_bounds(net->bounding_box, 0, 0, image);

            prev_net = net;
        };

        int total_places = integer_places + decimal_places;

        while(!reader.eof()) {

            std::string_view line = reader.read_line();

            // Skip empty lines
            if(line.empty()) {
                continue;
            }

            // M48 - start of header
            if(line.starts_with("M48")) {
                in_header = true;
                continue;
            }

            // End of header
            if(line[0] == '%' || line.starts_with("M95")) {
                in_header = false;
                continue;
            }

            // M30 / M00 - end of file
            if(line.starts_with("M30") || line.starts_with("M00")) {
                break;
            }

            // Comments
            if(line[0] == ';') {
                comments.push_back(std::string(line));

                // Check for ;FILE_FORMAT=I:D
                auto ff = line.find("FILE_FORMAT=");
                if(ff != std::string_view::npos) {
                    ff += 12;    // skip "FILE_FORMAT="
                    size_t pos = ff;
                    int i_part = parse_int(line, pos);
                    if(pos < line.size() && line[pos] == ':') {
                        pos += 1;
                        int d_part = parse_int(line, pos);
                        if(i_part > 0 && d_part > 0) {
                            integer_places = i_part;
                            decimal_places = d_part;
                            format_explicit = true;
                            decimal_divisor = std::pow(10.0, decimal_places);
                        }
                    }
                }
                continue;
            }

            if(in_header) {

                // Units: INCH or METRIC, with optional LZ/TZ
                if(line.starts_with("INCH")) {
                    units_inch = true;
                    if(line.find("TZ") != std::string_view::npos) {
                        trailing_zero_suppression = true;
                    } else {
                        trailing_zero_suppression = false;
                    }
                    update_derived();
                    continue;
                }
                if(line.starts_with("METRIC")) {
                    units_inch = false;
                    if(line.find("TZ") != std::string_view::npos) {
                        trailing_zero_suppression = true;
                    } else {
                        trailing_zero_suppression = false;
                    }
                    update_derived();
                    continue;
                }

                // Tool definition: T<n>F<f>S<s>C<diameter> or T<n>C<diameter>
                if(line[0] == 'T' && line.size() > 1 && line[1] >= '0' && line[1] <= '9') {

                    size_t pos = 1;
                    int tool_num = parse_int(line, pos);

                    // Find 'C' for diameter
                    auto c_pos = line.find('C', pos);
                    if(c_pos != std::string_view::npos) {
                        c_pos += 1;
                        std::string_view diam_str = line.substr(c_pos);
                        double diameter = 0.0;
                        auto [ptr, ec] = std::from_chars(diam_str.data(), diam_str.data() + diam_str.size(), diameter);
                        if(ec == std::errc()) {
                            // Convert diameter to mm
                            diameter *= unit_scale;

                            gerber_aperture *aperture = new gerber_aperture();
                            aperture->aperture_type = aperture_type_circle;
                            aperture->aperture_number = tool_num;
                            aperture->unit = units_inch ? unit_inch : unit_millimeter;
                            aperture->parameters.push_back(diameter);
                            image.apertures[tool_num] = aperture;

                            LOG_VERBOSE("Tool T{}: diameter {:g}mm", tool_num, diameter);
                        }
                    }
                    continue;
                }

                // FMAT command (ignore, we use the format from FILE_FORMAT or defaults)
                continue;
            }

            // Body parsing

            total_places = integer_places + decimal_places;

            // Tool selection in body: T<nn> (just digits, no F/S/C)
            if(line[0] == 'T' && line.size() > 1 && line[1] >= '0' && line[1] <= '9') {
                size_t pos = 1;
                current_tool = parse_int(line, pos);
                LOG_VERBOSE("Select tool T{}", current_tool);
                continue;
            }

            // G-codes that might appear in body (G90, G05, etc.) - skip if line is only a G-code
            if(line[0] == 'G' && line.find('X') == std::string_view::npos && line.find('Y') == std::string_view::npos) {
                continue;
            }

            // Coordinate line (may start with G90/G05 prefix before X/Y, or contain G85 for slots)
            if(line.find('X') != std::string_view::npos || line.find('Y') != std::string_view::npos) {

                size_t pos = 0;

                // Skip leading G-codes (e.g., G90, G05 at start of line)
                while(pos < line.size() && line[pos] == 'G') {
                    pos += 1;
                    parse_int(line, pos);    // skip the G-code number
                }

                // Parse first X,Y
                double x_mm = prev_x;
                double y_mm = prev_y;

                // Scan for X
                for(size_t i = pos; i < line.size(); ++i) {
                    if(line[i] == 'X') {
                        size_t dpos = i + 1;
                        auto digits = parse_digits(line, dpos);
                        x_mm = convert_coordinate(digits, total_places, trailing_zero_suppression, unit_scale, decimal_divisor);
                        break;
                    }
                    if(line[i] == 'G') {
                        break;    // stop before G85
                    }
                }

                // Scan for Y before any G85
                auto g85_pos = line.find("G85", pos);
                size_t y_search_end = (g85_pos != std::string_view::npos) ? g85_pos : line.size();
                for(size_t i = pos; i < y_search_end; ++i) {
                    if(line[i] == 'Y') {
                        size_t dpos = i + 1;
                        auto digits = parse_digits(line, dpos);
                        y_mm = convert_coordinate(digits, total_places, trailing_zero_suppression, unit_scale, decimal_divisor);
                        break;
                    }
                }

                // Check for G85 (slot)
                if(g85_pos != std::string_view::npos) {

                    // Parse end coordinates after G85
                    double x2_mm = x_mm;
                    double y2_mm = y_mm;
                    size_t slot_pos = g85_pos + 3;    // skip "G85"

                    for(size_t i = slot_pos; i < line.size(); ++i) {
                        if(line[i] == 'X') {
                            size_t dpos = i + 1;
                            auto digits = parse_digits(line, dpos);
                            x2_mm = convert_coordinate(digits, total_places, trailing_zero_suppression, unit_scale, decimal_divisor);
                            break;
                        }
                    }
                    for(size_t i = slot_pos; i < line.size(); ++i) {
                        if(line[i] == 'Y') {
                            size_t dpos = i + 1;
                            auto digits = parse_digits(line, dpos);
                            y2_mm = convert_coordinate(digits, total_places, trailing_zero_suppression, unit_scale, decimal_divisor);
                            break;
                        }
                    }

                    make_slot_net(x_mm, y_mm, x2_mm, y2_mm);
                    LOG_VERBOSE("Slot T{}: ({:g},{:g}) -> ({:g},{:g})", current_tool, x_mm, y_mm, x2_mm, y2_mm);
                } else {
                    // Round hole
                    make_flash_net(x_mm, y_mm);
                    LOG_VERBOSE("Drill T{}: ({:g},{:g})", current_tool, x_mm, y_mm);
                }

                prev_x = x_mm;
                prev_y = y_mm;
                continue;
            }
        }

        LOG_INFO("Parsed drill file: {} tools, {} entities", image.apertures.size(), entities.size());
        return ok;
    }

}    // namespace gerber_lib
