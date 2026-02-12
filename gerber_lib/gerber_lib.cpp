//////////////////////////////////////////////////////////////////////

#define _USE_MATH_DEFINES
#include <math.h>

#include <string>
#include <memory>
#include <span>
#include <algorithm>
#include <format>
#include <array>
#include <ranges>
#include <filesystem>

#include "gerber_error.h"
#include "gerber_util.h"
#include "gerber_lib.h"
#include "gerber_net.h"
#include "gerber_draw.h"
#include "gerber_aperture.h"
#include "gerber_image.h"
#include "gerber_reader.h"

#include <charconv>

LOG_CONTEXT("gerber_lib", debug);

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace gerber_lib;
    using namespace gerber_util;

    int hide_elements = 0

#if 0
    | hide_element_lines
    | hide_element_arcs
    | hide_element_circles
    | hide_element_rectangles
    | hide_element_ovals
    | hide_element_polygons
    | hide_element_outlines
    | hide_element_macros
#endif
        ;

    //////////////////////////////////////////////////////////////////////

    std::string to_lower(std::string_view s)
    {
        std::string r;
        r.reserve(s.size());
        for(char c : s) {
            if(c >= 'A' && c <= 'Z') {
                c |= 0x20;
            }
            r.push_back(c);
        }
        return r;
    }

    //////////////////////////////////////////////////////////////////////

    bool is_positive_integer(std::string_view sv)
    {
        if(sv.empty() || (sv[0] == '-' || sv[0] == '+')) {
            return false;
        }
        unsigned int value;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
        return ec == std::errc{} && ptr == sv.data() + sv.size();
    }

    //////////////////////////////////////////////////////////////////////

    bool all_digits(std::string_view sv)
    {
        if(sv.empty()) {
            return false;
        }
        for(auto const &c : sv) {
            if(!isdigit(c)) {
                return false;
            }
        }
        return true;
    }

    //////////////////////////////////////////////////////////////////////

    void add_trailing_zeros(int integer_part, int decimal_part, int length, int *coordinate)
    {
        // LOG_DEBUG("add_trailing_zeros({},{},{}) {}", integer_part, decimal_part, length, *coordinate);

        int omitted_value = integer_part + decimal_part - length;
        for(int x = 0; x < omitted_value; x++) {
            *coordinate *= 10;
        }
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code calculate_arc_mq(gerber_net *net, bool is_clockwise, vec2d const &center)
    {
        net->circle_segment.pos = net->start.add(center);
        vec2d d1 = center.scale(-1);
        vec2d d2 = net->end.subtract(net->circle_segment.pos);

        if(fabs(d1.x) < 1.0e-6) {
            d1.x = 0;
        }
        if(fabs(d1.y) < 1.0e-6) {
            d1.y = 0;
        }
        if(fabs(d2.x) < 1.0e-6) {
            d2.x = 0;
        }
        if(fabs(d2.y) < 1.0e-6) {
            d2.y = 0;
        }

        double alpha = rad_2_deg(atan2(d1.y, d1.x));
        double beta = rad_2_deg(atan2(d2.y, d2.x));

        double r = center.length() * 2.0;
        net->circle_segment.size = { r, r };

        if(alpha < 0.0) {
            alpha += 360.0;
            beta += 360.0;
        }
        if(beta < 0.0) {
            beta += 360.0;
        }
        if(is_clockwise) {
            if(alpha - beta < 1.0e-6) {
                beta -= 360.0;
            }
        } else {
            if(beta - alpha < 1.0e-6) {
                beta += 360.0;
            }
        }

        if(beta < alpha) {
            std::swap(alpha, beta);
        }

        net->circle_segment.start_angle = alpha;
        net->circle_segment.end_angle = beta;

        LOG_DEBUG("ARC: POS {}, SIZE: {}, START: {:g}, END: {:g}",
                  net->circle_segment.pos,
                  net->circle_segment.size,
                  net->circle_segment.start_angle,
                  net->circle_segment.end_angle);

        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code calculate_arc_sq(gerber_net *net, bool is_clockwise, vec2d const &center)
    {
        std::array centers{ vec2d{ net->start.x + center.x, net->start.y + center.y },
                            vec2d{ net->start.x + center.x, net->start.y - center.y },
                            vec2d{ net->start.x - center.x, net->start.y + center.y },
                            vec2d{ net->start.x - center.x, net->start.y - center.y } };

        double constexpr allowable_deviation = 0.0005;
        double best_deviation{ DBL_MAX };

        gerber_error_code rc = error_invalid_arc;

        for(auto const &c : centers) {

            vec2d d1 = c.subtract(net->start);
            vec2d d2 = c.subtract(net->end);

            double start_radius = d1.length();
            double end_radius = d2.length();

            double deviation = abs(start_radius - end_radius);

            if(deviation > best_deviation) {
                continue;
            }

            // YOINK: check len(d1) != 0 and len(d2) != 0

            double alpha = rad_2_deg(atan2(d1.y, d1.x));
            double beta = rad_2_deg(atan2(d2.y, d2.x));

            if(d1.x < 0.0) {
                alpha -= 360.0;
                if(alpha < 0) {
                    alpha += 360.0;
                }
            } else {
                alpha += 180.0;
            }

            if(d2.x < 0.0) {
                beta -= 360.0;
                if(beta < 0) {
                    beta += 360.0;
                }
            } else {
                beta += 180.0;
            }

            if(is_clockwise) {
                if(alpha == 0.0) {
                    alpha = 360.0;
                }
                if(beta == 360.0) {
                    beta = 0;
                }
                if(beta > 270.0 && alpha < 90.0) {
                    beta -= 360.0;
                }
            } else {
                if(alpha > 280.0 && beta < 90.0) {
                    alpha -= 360.0;
                }
            }
            if(abs(beta - alpha) > 90.0) {
                continue;
            }

            if(deviation < allowable_deviation) {
                best_deviation = deviation;
                net->circle_segment.start_angle = alpha;
                net->circle_segment.end_angle = beta;
                net->circle_segment.pos = c;
                net->circle_segment.size.x = (alpha < beta ? start_radius : end_radius) * 2.0;
                net->circle_segment.size.y = (alpha > beta ? start_radius : end_radius) * 2.0;
                rc = ok;
            }
        }
        return rc;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code calculate_arc(gerber_net *net, bool is_multi_quadrant, bool is_clockwise, vec2d const &center)
    {
        if(is_multi_quadrant) {
            return calculate_arc_mq(net, is_clockwise, center);
        }
        return calculate_arc_sq(net, is_clockwise, center);
    }

    //////////////////////////////////////////////////////////////////////

    void update_bounds(rect &bounds, matrix const &matrix, vec2d const &point)
    {
        bounds.expand_to_contain(transform_point(matrix, point));
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> void update_bounds(rect &bounds, matrix const &matrix, T points)
    {
        for(auto const &point : points) {
            update_bounds(bounds, matrix, point);
        }
    }

    //////////////////////////////////////////////////////////////////////

    std::optional<unsigned> get_uint(std::string_view sv)
    {
        if(!sv.empty()) {
            unsigned value = 0;
            auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
            if(ec == std::errc{} && ptr == sv.data() + sv.size()) {
                return value;
            }
        }
        return std::nullopt;
    }

}    // namespace

namespace gerber_lib
{
    char const *layer_type_name_friendly(layer::type_t t)
    {
        if(is_layer_type(t, layer::type_t::unknown)) {
            return "Unknown";
        }
        if(is_layer_type(t, layer::type_t::other)) {
            return "Other";
        }
        if(is_layer_type(t, layer::type_t::vcut)) {
            return "V-Cut";
        }
        if(is_layer_type(t, layer::type_t::board)) {
            return "Board";
        }
        if(is_layer_type(t, layer::type_t::outline)) {
            return "Outline";
        }
        if(is_layer_type(t, layer::type_t::mechanical)) {
            return "Mechanical";
        }
        if(is_layer_type(t, layer::type_t::info)) {
            return "Info";
        }
        if(is_layer_type(t, layer::type_t::keepout)) {
            return "Keep Out";
        }
        if(is_layer_type(t, layer::type_t::pads)) {
            return "Pads";
        }
        if(is_layer_type(t, layer::type_t::drill)) {
            return "Drill";
        }
        if(is_layer_type(t, layer::type_t::paste_top)) {
            return "Top paste";
        }
        if(is_layer_type(t, layer::type_t::pads_top)) {
            return "Top pads";
        }
        if(is_layer_type(t, layer::type_t::overlay_top)) {
            return "Top overlay";
        }
        if(is_layer_type(t, layer::type_t::soldermask_top)) {
            return "Top soldermask";
        }
        if(is_layer_type(t, layer::type_t::drill_top)) {
            return "[Top drill]";
        }
        if(is_layer_type(t, layer::type_t::copper_top)) {
            return "Top copper";
        }
        if(is_layer_type(t, layer::type_t::copper_inner)) {
            return "Inner copper";
        }
        if(is_layer_type(t, layer::type_t::copper_bottom)) {
            return "Bottom copper";
        }
        if(is_layer_type(t, layer::type_t::drill_bottom)) {
            return "[Bottom drill]";
        }
        if(is_layer_type(t, layer::type_t::soldermask_bottom)) {
            return "Bottom soldermask";
        }
        if(is_layer_type(t, layer::type_t::overlay_bottom)) {
            return "Bottom overlay";
        }
        if(is_layer_type(t, layer::type_t::pads_bottom)) {
            return "Bottom pads";
        }
        if(is_layer_type(t, layer::type_t::paste_bottom)) {
            return "Bottom paste";
        }
        return "Unclassified";
    }

    //////////////////////////////////////////////////////////////////////

    char const *layer_type_name(layer::type_t t)
    {
        if(is_layer_type(t, layer::type_t::unknown)) {
            return "unknown";
        }
        if(is_layer_type(t, layer::type_t::other)) {
            return "other";
        }
        if(is_layer_type(t, layer::type_t::vcut)) {
            return "vcut";
        }
        if(is_layer_type(t, layer::type_t::board)) {
            return "board";
        }
        if(is_layer_type(t, layer::type_t::outline)) {
            return "outline";
        }
        if(is_layer_type(t, layer::type_t::mechanical)) {
            return "mechanical";
        }
        if(is_layer_type(t, layer::type_t::info)) {
            return "info";
        }
        if(is_layer_type(t, layer::type_t::keepout)) {
            return "keepout";
        }
        if(is_layer_type(t, layer::type_t::pads)) {
            return "pads";
        }
        if(is_layer_type(t, layer::type_t::drill)) {
            return "drill";
        }
        if(is_layer_type(t, layer::type_t::paste_top)) {
            return "paste_top";
        }
        if(is_layer_type(t, layer::type_t::pads_top)) {
            return "pads_top";
        }
        if(is_layer_type(t, layer::type_t::overlay_top)) {
            return "overlay_top";
        }
        if(is_layer_type(t, layer::type_t::soldermask_top)) {
            return "soldermask_top";
        }
        if(is_layer_type(t, layer::type_t::drill_top)) {
            return "drill_top";
        }
        if(is_layer_type(t, layer::type_t::copper_top)) {
            return "copper_top";
        }
        if(is_layer_type(t, layer::type_t::copper_inner)) {
            return "copper_inner";
        }
        if(is_layer_type(t, layer::type_t::copper_bottom)) {
            return "copper_bottom";
        }
        if(is_layer_type(t, layer::type_t::drill_bottom)) {
            return "drill_bottom";
        }
        if(is_layer_type(t, layer::type_t::soldermask_bottom)) {
            return "soldermask_bottom";
        }
        if(is_layer_type(t, layer::type_t::overlay_bottom)) {
            return "overlay_bottom";
        }
        if(is_layer_type(t, layer::type_t::pads_bottom)) {
            return "pads_bottom";
        }
        if(is_layer_type(t, layer::type_t::paste_bottom)) {
            return "paste_bottom";
        }
        return "?unclassifiable!?";
    }

    //////////////////////////////////////////////////////////////////////

    layer::type_t gerber_file::classify() const
    {
        using namespace layer;
        auto path = std::filesystem::path(filename);
        std::string name(to_lower(path.filename().string()));
        std::string stem(to_lower(path.stem().string()));
        std::string extension(to_lower(path.extension().string()));

        // X2 Attributes
        for(auto const &pair : attributes) {
            std::string k = to_lower(pair.first);
            if(k == ".filefunction" || k == "filefunction") {
                std::string v(to_lower(pair.second));
                if(v.contains("copper")) {
                    if(v.contains("top")) {
                        return copper_top;
                    }
                    if(v.contains("bot")) {
                        return copper_bottom;
                    }
                    // Find the L## bit
                    std::vector<std::string_view> tokens;
                    tokenize(v, tokens, ",", tokenize_remove_empty);
                    for(auto token : tokens) {
                        if(tolower(token[0]) == 'l') {
                            auto n = get_uint(token.substr(1));
                            if(n.has_value()) {
                                return (type_t)((int)copper_inner + n.value());
                            }
                        }
                    }
                    return copper_inner;
                }
                if(v.contains("paste")) {
                    if(v.contains("bot")) {
                        return paste_bottom;
                    }
                    return paste_top;
                }
                if(v.contains("legend") || v.contains("silk")) {
                    if(v.contains("bot")) {
                        return overlay_bottom;
                    }
                    return overlay_top;
                }
                if(v.contains("soldermask")) {
                    if(v.contains("bot")) {
                        return soldermask_bottom;
                    }
                    return soldermask_top;
                }
                if(v.contains("drill")) {
                    return drill;
                }
                if(v.contains("other")) {
                    return other;
                }
                if(v.contains("pads")) {
                    return pads;
                }
                if(v.contains("vcut")) {
                    return vcut;
                }
                if(v.contains("keepout")) {
                    return keepout;
                }
                if(v.contains("outline")) {
                    return outline;
                }
                if(v.contains("profile")) {
                    return outline;
                }
            }
        }

        // Filename Patterns
        if(extension == ".gtl" || stem.ends_with("-f_cu")) {
            return copper_top;
        }
        if(extension == ".gbl" || stem.ends_with("-b_cu")) {
            return copper_bottom;
        }
        if(stem.ends_with("-in") && stem.ends_with("_cu")) {
            return copper_inner;
        }
        if(extension == ".gts" || stem.ends_with("-f_mask")) {
            return soldermask_top;
        }
        if(extension == ".gbs" || stem.ends_with("-b_mask")) {
            return soldermask_bottom;
        }
        if(extension == ".gtp" || stem.ends_with("-f_paste")) {
            return paste_top;
        }
        if(extension == ".gbp" || stem.ends_with("-b_paste")) {
            return paste_bottom;
        }
        if(extension == ".gko" || stem.ends_with("-margin") || stem.ends_with("-keepout")) {
            return keepout;
        }
        if(extension == ".gml" || stem.ends_with("-edge_cuts") || stem.ends_with("-outline")) {
            return outline;
        }
        if(extension == ".drl" || extension == ".txt") {
            return drill;
        }
        // Protel inner layers are a hassle (.g2, .g3, .g12 etc)
        if(extension.size() > 2 && extension[1] == 'g') {
            auto n = get_uint(std::string_view(extension).substr(2));
            if(n.has_value()) {
                return (type_t)((int)copper_inner + n.value());
            }
        }

        // JLCPCB production files
        if(name == "tl") {
            return copper_top;
        }
        if(name == "bl") {
            return copper_bottom;
        }
        if(name == "to") {
            return overlay_top;
        }
        if(name == "bo") {
            return overlay_bottom;
        }
        if(name == "bs") {
            return soldermask_bottom;
        }
        if(name == "ts") {
            return soldermask_top;
        }
        if(name == "drl") {
            return drill;
        }
        if(name == "ko") {
            return keepout;
        }
        if(name == "vcut") {
            return vcut;
        }
        // "l###" is inner layer
        if(name[0] == 'l') {
            auto n = get_uint(std::string_view(name).substr(1));
            if(n.has_value()) {
                return (type_t)((int)copper_inner + n.value());
            }
        }

        // Comments
        for(const auto &comment : comments) {
            std::string c(to_lower(comment));
            if(c.contains("keepout") || c.contains("keep-out")) {
                return keepout;
            }
            if(c.contains("paste")) {
                if(c.contains("top")) {
                    return paste_top;
                }
                if(c.contains("bot")) {
                    return paste_bottom;
                }
            }
        }
        return unknown;
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_file::cleanup()
    {
        attributes.clear();
        filename = std::string{};
        image.cleanup();
        stats.cleanup();
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_file::reset()
    {
        cleanup();
        image.file_type = file_type_rs274x;
        image.gerber = this;
        gerber_net *current_net = new gerber_net(&image);
        state.level = image.levels[0];
        state.net_state = image.net_states[0];
        current_net->level = state.level;
        current_net->net_state = state.net_state;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::do_parse()
    {
        filename = reader.filename;
        image.gerber = this;
        CHECK(parse_gerber_segment(image.nets[0]));
        LOG_VERBOSE("Parsing complete after {} lines, found {} entities", reader.line_number, entities.size());
        layer_type = classify();
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_file(char const *file_path)
    {
        reset();
        CHECK(reader.open(file_path));
        return do_parse();
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_memory(char const *data, size_t size)
    {
        reset();
        CHECK(reader.open(data, size));
        return do_parse();
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_g_code()
    {
        int code;
        CHECK(reader.get_int(&code));

        // LOG_DEBUG("G code {}", code);

        switch(code) {

        // Move ... Is this doing anything really? - Deprecated.
        case 0:
            stats.g0 += 1;
            break;

        // Linear Interpolation (1X scale)
        case 1:
            state.interpolation = interpolation_linear;
            stats.g1 += 1;
            break;

        // Clockwise Linear Interpolation
        case 2:
            state.interpolation = interpolation_clockwise_circular;
            stats.g2 += 1;
            break;

        // Counter Clockwise Linear Interpolation.
        case 3:
            state.interpolation = interpolation_counterclockwise_circular;
            stats.g3 += 1;
            break;

        // Comment
        case 4: {
            stats.g4 += 1;
            std::string comment;
            CHECK(reader.read_until(&comment, '*'));
            LOG_VERBOSE("Comment({}): {}", reader.line_number, comment);
        } break;

        // Turn on Region Fill
        case 36:
            state.previous_interpolation = state.interpolation;
            state.interpolation = interpolation_region_start;
            state.changed();
            stats.g36 += 1;
            break;

        // Turn off Region Fill
        case 37:
            state.interpolation = interpolation_region_end;
            state.changed();
            stats.g37 += 1;
            break;

        // Select aperture - Deprecated.
        case 54: {
            char c;
            CHECK(reader.read_char(&c));
            if(c != 'D') {
                return stats.error(reader, error_unexpected_code, "after G54, expected 'D', got {}", string_from_char(c));
            }

            int aperture_number;
            CHECK(reader.get_int(&aperture_number));

            if(aperture_number < 10 || aperture_number > max_num_apertures) {
                return stats.error(reader, error_bad_aperture_number, "D{} out of bounds", aperture_number);
            }

            state.current_aperture = aperture_number;
            stats.g54 += 1;
        } break;

        // Prepare for flash - Deprecated.
        case 55:
            stats.g55 += 1;
            break;

        // Specify inches - Deprecated.
        case 70:
            state.net_state->unit = unit_inch;
            stats.g70 += 1;
            break;

        // Specify millimeters  - Deprecated.
        case 71:
            state.net_state->unit = unit_millimeter;
            stats.g71 += 1;
            break;

        // Disable 360 circular interpolation.
        case 74:
            state.is_multi_quadrant = false;
            stats.g74 += 1;
            break;

        // Enable 360 circular interpolation.
        case 75:
            state.is_multi_quadrant = true;
            stats.g75 += 1;
            break;

        // Specify absolute format - Deprecated.
        case 90:
            image.format.coordinate = coordinate_absolute;
            stats.g90 += 1;
            break;

        // Specify incremental format - Deprecated.
        case 91:
            image.format.coordinate = coordinate_incremental;
            stats.g91 += 1;
            break;

        default:
            stats.error(reader, error_unknown_code, "Unknown code G{}", code);
            stats.unknown_g_codes += 1;
            break;
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_d_code()
    {
        LOG_CONTEXT("parse_d_code", info);

        int code;
        CHECK(reader.get_int(&code));

        switch(code) {

        // Exposure on.
        case 1:
            state.aperture_state = aperture_state_on;
            state.changed();
            stats.d1 += 1;
            break;

        // Exposure off.
        case 2:
            state.aperture_state = aperture_state_off;
            state.changed();
            stats.d2 += 1;
            break;

        // Flash aperture.
        case 3:
            state.aperture_state = aperture_state_flash;
            state.changed();
            stats.d3 += 1;
            break;

        // Aperture id in use.
        default:
            if(code >= 10 && code <= max_num_apertures) {
                LOG_DEBUG("Using aperture {} at line {}", code, reader.line_number);
                state.current_aperture = code;
            } else {
                stats.error(reader, error_bad_aperture_number, "D{} out of bounds", code);
                stats.d_code_errors += 1;
            }
            state.changed(false);
            break;
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    bool gerber_file::parse_m_code()
    {
        LOG_CONTEXT("M_code", none);

        int code;
        CHECK(reader.get_int(&code));

        LOG_DEBUG("M code {}", code);

        switch(code) {

        case 0:
            // 'optional stop', same as M02
            stats.m0 += 1;
            return true;

        case 1:
            // no effect
            stats.m1 += 1;
            return false;

        case 2:
            // M02 - end of file
            stats.m2 += 1;
            return true;

        default:
            stats.error(reader, error_invalid_code);
            return false;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // !!! Always returns after it eats the final %
    // SO....
    // If it's not a macro, that's simple
    // But if it's a macro, that's a bit more... complex?

    gerber_error_code gerber_file::parse_rs274x(gerber_net *net)
    {
        LOG_CONTEXT("RS274X", info);

        while(true) {
            double unit_scale{ 1.0 };

            if(state.net_state->unit == unit_inch) {
                unit_scale = 25.4;
            }

            uint32_t command;
            CHECK(reader.read_short(&command, 2));

            LOG_DEBUG("command {}", string_from_uint32(command));

            switch(command) {

                //////////////////////////////////////////////////////////////////////
                // AM: aperture macro

            case 'AM': {

                auto macro = std::make_unique<gerber_aperture_macro>();
                CHECK(macro->parse_aperture_macro(reader));
                LOG_DEBUG("AM {}", *macro);
                image.aperture_macros.push_back(macro.release());
                return ok;

            } break;    // redundant, for the linter

                //////////////////////////////////////////////////////////////////////
                // AD: aperture definition

            case 'AD': {
                auto aperture = std::make_unique<gerber_aperture>();

                if(parse_aperture_definition(aperture.get(), &image, unit_scale) == ok) {

                    LOG_DEBUG("AD {}", *aperture);

                    int aperture_number = aperture->aperture_number;

                    if(aperture_number >= 0 && aperture_number <= max_num_apertures) {

                        aperture->unit = state.net_state->unit;

                        if(image.apertures.contains(aperture_number)) {
                            stats.error(reader, error_duplicate_aperture_number, "aperture {} already defined, overwriting", aperture_number);
                            delete image.apertures[aperture_number];
                            // image.apertures.erase(aperture_number);
                        }

                        image.apertures[aperture_number] = aperture.release();

                        // stats.add_aperture(-1, aperture_number, aperture->aperture_type, aperture->parameters);
                        stats.add_new_d_list(aperture_number);

                        if(aperture_number < min_aperture) {
                            stats.error(reader, error_bad_aperture_number, "{}, must be >= {}, using {}", aperture_number, min_aperture, min_aperture);
                            aperture_number = min_aperture;
                        }
                        LOG_DEBUG("Set net aperture to {}", aperture_number);
                        net->aperture = aperture_number;

                    } else {

                        return stats.error(reader, error_bad_aperture_number, "{}, must be >= {}, <= {}", aperture_number, 0, max_num_apertures);
                    }
                }

            } break;

                //////////////////////////////////////////////////////////////////////
                // AS: axis select

            case 'AS': {

                state.net_state = new gerber_net_state(&image);

                CHECK(reader.read_short(&command, 4));

                switch(command) {

                case 'AXBY':
                    LOG_DEBUG("axis_select_none");
                    state.net_state->axis_select = axis_select_none;
                    break;

                case 'AYBX':
                    LOG_DEBUG("axis_select_swap_ab");
                    state.net_state->axis_select = axis_select_swap_ab;
                    break;

                default:
                    return stats.error(reader, error_invalid_axis_select, "expected [AXBY|AYBX], got {}", string_from_uint32(command));
                }
            } break;

            case 'TO': {
                std::string obj_attr;
                reader.read_until(&obj_attr, '*');
                std::vector<std::string> tokens;
                tokenize(obj_attr, tokens, ",", tokenize_keep_empty);
                if(tokens.size() < 2) {
                    stats.error(reader, error_malformed_command, "expected attribute_name,attribute_value, got {}", obj_attr);
                } else {
                    std::string s = join(std::span(tokens).subspan(1), ",");
                    LOG_DEBUG("TOKEN ATTR[{}] = {}", tokens[0], s);
                    attributes[tokens[0]] = s;
                }
            } break;

            case 'TD': {
                std::string attribute_to_clear;
                reader.read_until(&attribute_to_clear, '*');
                if(attribute_to_clear.empty()) {
                    LOG_DEBUG("Clear attribute dictionary");
                    attributes.clear();
                } else {
                    LOG_DEBUG("Delete attribute: \"{}\"", attribute_to_clear);
                    auto f = attributes.find(attribute_to_clear);
                    if(f == attributes.end()) {
                        stats.error(reader, error_missing_attribute, "Can't find {}", attribute_to_clear);
                    } else {
                        attributes.erase(attribute_to_clear);
                    }
                }
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // FS: format specification

            case 'FS': {

                char c;
                CHECK(reader.read_char(&c));

                switch(c) {

                case 'L':
                    LOG_DEBUG("omit_zeros_leading");
                    image.format.omit_zeros = omit_zeros_leading;
                    break;

                case 'T':
                    LOG_DEBUG("omit_zeros_trailing");
                    image.format.omit_zeros = omit_zeros_trailing;
                    break;

                case 'D':
                    LOG_DEBUG("omit_zeros_explicit");
                    image.format.omit_zeros = omit_zeros_explicit;
                    break;

                default:
                    return stats.error(reader, error_invalid_omit_zeros, "expected [L|T|D], got {}", string_from_char(c));
                }

                CHECK(reader.read_char(&c));

                switch(c) {

                case 'A':
                    LOG_DEBUG("coordinate_absolute");
                    image.format.coordinate = coordinate_absolute;
                    break;

                case 'I':
                    LOG_DEBUG("coordinate_incremental");
                    image.format.coordinate = coordinate_incremental;
                    break;

                default:
                    return stats.error(reader, error_invalid_coordinate_setting, "expected [A|I], got {}", string_from_char(c));
                }

                CHECK(reader.read_char(&c));

                while(c != '*') {

                    switch(c) {

                    case 'N':
                        CHECK(reader.read_char(&c));
                        if(isdigit(c)) {
                            image.format.sequence_number_limit = c - '0';
                            LOG_DEBUG("sequence_number_limit = {}", image.format.sequence_number_limit);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit, got {}", string_from_char(c));
                        }
                        break;

                    case 'G':
                        CHECK(reader.read_char(&c));
                        if(isdigit(c)) {
                            image.format.general_function_limit = c - '0';
                            LOG_DEBUG("general_function_limit = {}", image.format.general_function_limit);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit, got {}", string_from_char(c));
                        }
                        break;

                    case 'D':
                        CHECK(reader.read_char(&c));
                        if(isdigit(c)) {
                            image.format.plot_function_limit = c - '0';
                            LOG_DEBUG("plot_function_limit = {}", image.format.plot_function_limit);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit, got {}", string_from_char(c));
                        }
                        break;

                    case 'M':
                        CHECK(reader.read_char(&c));
                        if(isdigit(c)) {
                            image.format.misc_function_limit = c - '0';
                            LOG_DEBUG("misc_function_limit = {}", image.format.misc_function_limit);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit, got {}", string_from_char(c));
                        }
                        break;

                    case 'X':
                        CHECK(reader.read_char(&c));
                        if(c >= '0' && c <= '6') {
                            image.format.integral_part_x = c - '0';
                            LOG_DEBUG("integral_part_x = {}", image.format.integral_part_x);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit 0..6, got {}", string_from_char(c));
                        }
                        CHECK(reader.read_char(&c));
                        if(c >= '0' && c <= '6') {
                            image.format.decimal_part_x = c - '0';
                            LOG_DEBUG("decimal_part_x = {}", image.format.decimal_part_x);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit 0..6, got {}", string_from_char(c));
                        }
                        break;

                    case 'Y':
                        CHECK(reader.read_char(&c));
                        if(c >= '0' && c <= '6') {
                            image.format.integral_part_y = c - '0';
                            LOG_DEBUG("integral_part_y = {}", image.format.integral_part_y);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit 0..6, got {}", string_from_char(c));
                        }
                        CHECK(reader.read_char(&c));
                        if(c >= '0' && c <= '6') {
                            image.format.decimal_part_y = c - '0';
                            LOG_DEBUG("decimal_part_y = {}", image.format.decimal_part_y);
                        } else {
                            return stats.error(reader, error_out_of_range, "expected digit 0..6, got {}", string_from_char(c));
                        }
                        break;

                    default:
                        return stats.error(reader, error_invalid_format_specification, "expected [N|G|D|M|X|Y], got {}", string_from_char(c));
                    }
                    CHECK(reader.read_char(&c));
                }
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // MI: mirror state

            case 'MI': {

                state.net_state = new gerber_net_state(&image);

                char c;
                CHECK(reader.read_char(&c));

                gerber_mirror_state mirror = mirror_state_none;

                while(c != '*') {

                    switch(c) {

                    case 'A':
                        CHECK(reader.read_char(&c));
                        if(c == '1') {
                            mirror = mirror_state_flip_a;
                        } else if(c != '0') {
                            return stats.error(reader, error_invalid_mirror_state, "for axis A, expected [0|1], got {}", string_from_char(c));
                        }
                        break;

                    case 'B':
                        CHECK(reader.read_char(&c));
                        if(c == '1') {
                            if(mirror == mirror_state_flip_a) {
                                mirror = mirror_state_flip_ab;
                            } else {
                                mirror = mirror_state_flip_b;
                            }
                        } else if(c != '0') {
                            return stats.error(reader, error_invalid_mirror_state, "for axis B, expected [0|1], got {}", string_from_char(c));
                        }
                        break;

                    default:
                        return stats.error(reader, error_invalid_axis, "for MI, expected [A|B], got {}", string_from_char(c));
                    }
                    CHECK(reader.read_char(&c));
                }
                state.net_state->mirror_state = mirror;
                LOG_DEBUG("Mirror state: {}", mirror);
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // MO: mode (MM or INCH units, basically)

            case 'MO': {

                uint32_t rs_command;
                CHECK(reader.read_short(&rs_command, 2));

                gerber_unit unit{ unit_unspecified };

                switch(rs_command) {

                case 'IN':
                    unit = unit_inch;
                    break;

                case 'MM':
                    unit = unit_millimeter;
                    break;

                default:
                    return stats.error(reader, error_invalid_mode, "expected [IN|MM], got {}", string_from_uint32(command));
                }

                if(unit != unit_unspecified) {
                    state.net_state = new gerber_net_state(&image);
                    state.net_state->unit = unit;
                }
                LOG_DEBUG("Units: {}", unit);
            } break;

                //////////////////////////////////////////////////////////////////////
                // OF: offset

            case 'OF': {

                char c;
                CHECK(reader.read_char(&c));

                while(c != '*') {

                    double offset;
                    CHECK(reader.get_double(&offset));
                    offset *= unit_scale;

                    switch(c) {

                    case 'A':
                        state.net_state->offset.x = offset;
                        break;

                    case 'B':
                        state.net_state->offset.y = offset;
                        break;

                    default:
                        return stats.error(reader, error_invalid_offset, "expected [A|B], got {}", string_from_uint32(c));
                    }

                    CHECK(reader.read_char(&c));
                }
                LOG_DEBUG("Image offset: {},{}", state.net_state->offset.x, state.net_state->offset.y);
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // IF: include file - NOT SUPPORTED

            case 'IF': {
                return stats.error(reader, error_not_supported, "IF (include files) not supported");
            } break;

                //////////////////////////////////////////////////////////////////////
                // SF: scale factor

            case 'SF': {

                char c;
                CHECK(reader.read_char(&c));

                while(c != '*') {

                    double scale;
                    CHECK(reader.get_double(&scale));
                    scale *= unit_scale;

                    switch(c) {

                    case 'A':
                        state.net_state->scale.x = scale;
                        break;

                    case 'B':
                        state.net_state->scale.y = scale;
                        break;

                    default:
                        return stats.error(reader, error_invalid_axis, "for SF, expected [A|B], got {}", string_from_char(c));
                    }
                    CHECK(reader.read_char(&c));
                }
                LOG_DEBUG("Image scale factor: {},{}", state.net_state->scale.x, state.net_state->scale.y);
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // IO: image offset

            case 'IO': {

                char c;
                CHECK(reader.read_char(&c));

                while(c != '*') {

                    double offset;
                    CHECK(reader.get_double(&offset));
                    offset *= unit_scale;

                    switch(c) {

                    case 'A':
                        image.info.offset_a = offset;
                        break;

                    case 'B':
                        image.info.offset_b = offset;
                        break;

                    default:
                        return stats.error(reader, error_invalid_axis, "for IO, expected [A|B], got {}", string_from_char(c));
                    }
                    CHECK(reader.read_char(&c));
                }
                LOG_DEBUG("Image offset: A:{}, B:{}", image.info.offset_a, image.info.offset_b);
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // IJ: image justification

            case 'IJ': {

                char c;
                CHECK(reader.read_char(&c));

                image.info.justify_a = image_justify_lower_left;
                image.info.justify_b = image_justify_lower_left;
                image.info.image_justify_offset_a = 0.0;
                image.info.image_justify_offset_b = 0.0;

                while(c != '*') {

                    switch(c) {

                    case 'A':
                        CHECK(reader.read_char(&c));
                        if(c == 'C') {
                            image.info.justify_a = image_justify_centre;
                        } else if(c == 'L') {
                            image.info.justify_a = image_justify_lower_left;
                        } else {
                            reader.rewind(1);
                            double offset;
                            CHECK(reader.get_double(&offset));
                            image.info.image_justify_offset_a = offset * unit_scale;
                        }
                        break;

                    case 'B':
                        CHECK(reader.read_char(&c));
                        if(c == 'C') {
                            image.info.justify_b = image_justify_centre;
                        } else if(c == 'L') {
                            image.info.justify_b = image_justify_lower_left;
                        } else {
                            reader.rewind(1);
                            double offset;
                            CHECK(reader.get_double(&offset));
                            image.info.image_justify_offset_b = offset * unit_scale;
                        }
                        break;

                    default:
                        return stats.error(reader, error_invalid_axis, "for IJ, expected [A|B], got {}", string_from_char(c));
                    }
                    CHECK(reader.read_char(&c));
                }
                LOG_DEBUG("Image justify: A:{} (offset {}) B:{} (offset {})",
                          image.info.justify_a,
                          image.info.image_justify_offset_a,
                          image.info.justify_b,
                          image.info.image_justify_offset_b);
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // IN: image name

            case 'IN': {
                CHECK(reader.read_until(&image.info.image_name, '*'));
                LOG_DEBUG("Image name: {}", image.info.image_name);
            } break;

                //////////////////////////////////////////////////////////////////////
                // IP: image polarity

            case 'IP': {

                uint32_t cmd;
                CHECK(reader.read_short(&cmd, 3));

                switch(cmd) {

                case 'POS':
                    image.info.polarity = polarity_positive;
                    break;

                case 'NEG':
                    image.info.polarity = polarity_negative;
                    break;

                default:
                    return stats.error(reader, error_invalid_polarity, "expected [POS|NEG], got {}", string_from_uint32(cmd));
                }
                LOG_DEBUG("Image polarity: {}", image.info.polarity);
            } break;

                //////////////////////////////////////////////////////////////////////
                // IR: image rotation

            case 'IR': {

                int rotation;
                CHECK(reader.get_int(&rotation));

                if(rotation % 90 == 0) {
                    image.info.image_rotation = static_cast<double>(rotation);
                } else {
                    return stats.error(reader, error_invalid_rotation, "expected [0|90|180|270], got {}", rotation);
                }
                LOG_DEBUG("Image rotation: {}", rotation);
            } break;

                //////////////////////////////////////////////////////////////////////
                // PF: plotter film

            case 'PF': {
                CHECK(reader.read_until(&image.info.plotter_film, '*'));
            } break;

                //////////////////////////////////////////////////////////////////////
                // AB: aperture block - NOT SUPPORTED

            case 'AB': {

                return stats.error(reader, error_unsupported_command, "{} (block aperture) not supported", string_from_uint32(command));
            }

                //////////////////////////////////////////////////////////////////////
                // LN: level name

            case 'LN': {
                std::string name;
                CHECK(reader.read_until(&name, '*'));
                state.level = new gerber_level(&image);
                state.level->name = name;
                LOG_DEBUG("Level name: {}", state.level->name);
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // LP: level polarity

            case 'LP': {
                char c;
                CHECK(reader.read_char(&c));

                switch(c) {

                case 'D':
                    state.level = new gerber_level(&image);
                    state.level->polarity = polarity_dark;
                    break;

                case 'C':
                    state.level = new gerber_level(&image);
                    state.level->polarity = polarity_clear;
                    break;

                default:
                    return stats.error(reader, error_invalid_level_polarity, "expected [D|C], got {}", string_from_char(c));
                }
                LOG_DEBUG("Polarity: {}", state.level->polarity);
            } break;

                //////////////////////////////////////////////////////////////////////
                // KO: knockout

            case 'KO': {

                state.level = new gerber_level(&image);

                update_knockout_measurements();

                knockout_measure = false;

                char c;
                CHECK(reader.read_char(&c));
                switch(c) {

                case '*':
                    state.level->knockout.knockout_type = knockout_type_no_knockout;
                    reader.rewind(1);
                    break;

                case 'C':
                    state.level->knockout.polarity = polarity_clear;
                    break;

                case 'D':
                    state.level->knockout.polarity = polarity_dark;
                    break;

                default:
                    return stats.error(reader, error_invalid_knockout_polarity, "expected [D|C], got {}", string_from_char(c));
                }

                LOG_DEBUG("KNOCKOUT: {}", state.level->knockout.knockout_type);

                state.level->knockout.lower_left = { 0, 0 };
                state.level->knockout.size = { 0, 0 };
                state.level->knockout.border = 0.0;
                state.level->knockout.first_instance = true;

                CHECK(reader.read_char(&c));

                while(c != '*') {

                    double d;
                    CHECK(reader.get_double(&d));
                    d *= unit_scale;

                    switch(c) {

                    case 'X':
                        state.level->knockout.knockout_type = knockout_type_fixed_knockout;
                        state.level->knockout.lower_left.x = d;
                        break;

                    case 'Y':
                        state.level->knockout.knockout_type = knockout_type_fixed_knockout;
                        state.level->knockout.lower_left.y = d;
                        break;

                    case 'I':
                        state.level->knockout.knockout_type = knockout_type_fixed_knockout;
                        state.level->knockout.size.x = d;
                        break;

                    case 'J':
                        state.level->knockout.knockout_type = knockout_type_fixed_knockout;
                        state.level->knockout.size.y = d;
                        break;

                    case 'K':
                        state.level->knockout.knockout_type = knockout_type_border;
                        state.level->knockout.border = d;

                        // This is a bordered knockout, so we need to start measuring the size of the area bordering all future components.
                        knockout_measure = true;
                        knockout_limit_min = { DBL_MAX, DBL_MAX };
                        knockout_limit_max = { DBL_MIN, DBL_MIN };
                        knockout_level = *state.level;    // Save a copy of this level for future access.
                        break;

                    default:
                        return stats.error(reader, error_invalid_knockout_variable, "expected [X|Y|I|J|K], got {}", string_from_char(c));
                    }
                    CHECK(reader.read_char(&c));
                }
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // SR: step & repeat

            case 'SR': {

                state.level = new gerber_level(&image);

                state.level->step_and_repeat.pos = { 1.0, 1.0 };
                state.level->step_and_repeat.distance = { 0.0, 0.0 };

                char c;
                CHECK(reader.read_char(&c));

                while(c != '*') {

                    int i;
                    double d;

                    switch(c) {

                    case 'X':
                        CHECK(reader.get_int(&i));
                        if(i == 0) {
                            i = 1;
                        }
                        state.level->step_and_repeat.pos.x = static_cast<double>(i);
                        break;

                    case 'Y':
                        CHECK(reader.get_int(&i));
                        if(i == 0) {
                            i = 1;
                        }
                        state.level->step_and_repeat.pos.y = static_cast<double>(i);
                        break;

                    case 'I':
                        CHECK(reader.get_double(&d));
                        state.level->step_and_repeat.distance.x = d;
                        break;

                    case 'J':
                        CHECK(reader.get_double(&d));
                        state.level->step_and_repeat.distance.y = d;
                        break;

                    default:
                        return stats.error(reader, error_invalid_step_and_repeat, "expected [X|Y|I|J], got {}", string_from_char(c));
                    }
                    CHECK(reader.read_char(&c));
                }
                LOG_DEBUG("Step and repeat: POS: {},{}, DISTANCE: {},{}",
                          state.level->step_and_repeat.pos.x,
                          state.level->step_and_repeat.pos.y,
                          state.level->step_and_repeat.distance.x,
                          state.level->step_and_repeat.distance.y);
                reader.rewind(1);
            } break;

                //////////////////////////////////////////////////////////////////////
                // TF: file attributes

            case 'TF': {
                CHECK(parse_tf_code());
            } break;

                //////////////////////////////////////////////////////////////////////
                // TA: aperture attributes

            case 'TA': {
                struct gerber_aperture_attribute
                {
                    std::string name;
                    std::string value;
                };
                std::string field;
                CHECK(reader.read_until(&field, '*'));
                std::vector<std::string> tokens;
                tokenize(field, tokens, ",", tokenize_keep_empty);
                if(tokens.empty()) {
                    LOG_ERROR("Empty TA field!?");
                } else {
                    std::string value;
                    if(tokens.size() > 1) {
                        value = tokens[1];
                    }
                    LOG_VERBOSE("TA: {}=\"{}\"", tokens[0], value);
                }
            } break;

                //////////////////////////////////////////////////////////////////////

            default:
                return stats.error(reader, error_unknown_command, "{}", string_from_uint32(command));
            }

            // ignore everything up to (but not including) the *
            CHECK(reader.read_until(nullptr, '*'));

            if(reader.eof()) {
                return stats.error(reader, error_unterminated_command, "expected *, got EOF");
            }

            // skip the trailing *
            reader.skip(1);

            // % means end of RS274X commands
            char c;
            CHECK(reader.peek(&c));
            if(c == '%') {
                reader.skip(1);
                return ok;
            }
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_tf_code()
    {
        char c;
        CHECK(reader.read_char(&c));
        if(c != '.') {
            LOG_ERROR("can't parse (TF missing period)");
            return error_invalid_file_attributes;
        }
        std::string tf;
        CHECK(reader.read_until(&tf, '*'));
        std::vector<std::string> tokens;
        tokenize(tf, tokens, ",", tokenize_remove_empty);
        if(tokens.size() < 2) {
            LOG_ERROR("can't parse {} for TF", tf);
            return error_invalid_file_attributes;
        }
        std::string attribute_name = tokens[0];
        std::string attribute_value = join(std::span(tokens.data() + 1, tokens.size() - 1), ",");

        LOG_VERBOSE("Attribute:{}={}", attribute_name, attribute_value);
        attributes[attribute_name] = attribute_value;
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_file::update_knockout_measurements()
    {
        if(knockout_measure) {
            knockout_level.knockout.lower_left = knockout_limit_min;
            knockout_level.knockout.size = knockout_limit_max.subtract(knockout_limit_min);
            knockout_measure = false;
        }
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_aperture_definition(gerber_aperture *aperture, gerber_image *cur_image, double unit_scale)
    {
        LOG_CONTEXT("parse_aperture", debug);

        char c;
        CHECK(reader.read_char(&c));
        if(c != 'D') {
            LOG_ERROR("Invalid char after AD, found: {}", string_from_char(c));
            return error_unexpected_input;
        }
        int aperture_id;
        CHECK(reader.get_int(&aperture_id));

        std::string definition;
        CHECK(reader.read_until(&definition, '*'));

        std::vector<std::string> tokens;
        tokenize(definition, tokens, ",", tokenize_remove_empty);

        if(tokens.empty()) {
            LOG_ERROR("Bad aperture definition: {}", definition);
            return error_invalid_aperture_definition;
        }

        if(tokens[0].size() == 1) {
            switch(tokens[0][0]) {
            case 'C':
                aperture->aperture_type = aperture_type_circle;
                break;
            case 'R':
                aperture->aperture_type = aperture_type_rectangle;
                break;
            case 'O':
                aperture->aperture_type = aperture_type_oval;
                break;
            case 'P':
                aperture->aperture_type = aperture_type_polygon;
                break;
            }
            LOG_DEBUG("Aperture definition {} is a {}", aperture_id, aperture->aperture_type);

        } else {
            aperture->aperture_type = aperture_type_macro;
            for(gerber_aperture_macro *macro : cur_image->aperture_macros) {
                if(macro->name.compare(tokens[0]) == 0) {
                    aperture->aperture_macro = macro;
                    break;
                }
            }
            if(aperture->aperture_macro == nullptr) {
                LOG_ERROR("Unknown aperture macro: {}", tokens[0]);
                return error_unknown_aperture_macro;
            }
            LOG_DEBUG("Aperture definition {} is macro {}", aperture_id, aperture->aperture_macro->name);
        }
        int parameter_index = 0;
        size_t token_count = tokens.size();
        for(size_t token_index = 1; token_index < token_count; ++token_index) {
            if(parameter_index == max_num_aperture_parameters) {
                LOG_ERROR("Parameter count {} exceeds max allowed ({})", parameter_index, max_num_aperture_parameters);
                return error_excessive_macro_parameters;
            }
            std::vector<std::string> parameters;
            tokenize(tokens[token_index], parameters, "Xx", tokenize_remove_empty);
            for(std::string const &s : parameters) {
                auto value = double_from_string_view(s);
                if(!value.has_value()) {
                    LOG_ERROR("Invalid number in aperture parameters: \"{}\" ({})", s, (int)value.error());
                    return error_invalid_number;
                }
                if(aperture->parameters.size() <= static_cast<size_t>(parameter_index)) {
                    aperture->parameters.resize(static_cast<size_t>(parameter_index) + 1);
                }
                aperture->parameters[parameter_index] = value.value();
                LOG_DEBUG("parameter[{}] = {}", parameter_index, value.value());
                parameter_index += 1;
            }
        }
        switch(aperture->aperture_type) {
        case aperture_type_macro:
            CHECK(aperture->execute_aperture_macro());
            break;
        case aperture_type_circle:
            aperture->parameters[0] *= unit_scale;
            if(aperture->parameters.size() > 1) {
                aperture->parameters[1] *= unit_scale;
            }
            break;
        case aperture_type_oval:
        case aperture_type_rectangle:
            aperture->parameters[0] *= unit_scale;
            aperture->parameters[1] *= unit_scale;
            if(aperture->parameters.size() > 2) {
                aperture->parameters[2] *= unit_scale;
            }
            break;
        case aperture_type_polygon:
            aperture->parameters[0] *= unit_scale;
            if(aperture->parameters.size() > 3) {
                aperture->parameters[3] *= unit_scale;
            }
            break;
        default:
            break;
        }
        aperture->aperture_number = aperture_id;
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_file::update_net_bounds(rect &bounds, std::vector<vec2d> const &points) const
    {
        update_bounds(bounds, aperture_matrix, points);
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_file::update_net_bounds(rect &bounds, double x, double y, double w, double h) const
    {
        update_bounds(bounds, aperture_matrix, { x - w, y - h });
        update_bounds(bounds, aperture_matrix, { x + w, y + h });
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_file::update_image_bounds(rect &bounds, double repeat_offset_x, double repeat_offset_y, gerber_image &cur_image) const
    {
        double minx = bounds.min_pos.x + repeat_offset_x;
        double maxx = bounds.max_pos.x + repeat_offset_x;
        double miny = bounds.min_pos.y + repeat_offset_y;
        double maxy = bounds.max_pos.y + repeat_offset_y;

        if(minx < cur_image.info.extent.min_pos.x) {
            cur_image.info.extent.min_pos.x = minx;
        }

        if(maxx > cur_image.info.extent.max_pos.x) {
            cur_image.info.extent.max_pos.x = maxx;
        }

        if(miny < cur_image.info.extent.min_pos.y) {
            cur_image.info.extent.min_pos.y = miny;
        }

        if(maxy > cur_image.info.extent.max_pos.y) {
            cur_image.info.extent.max_pos.y = maxy;
        }
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::get_aperture_points(gerber_macro_parameters const &macro, gerber_net *net, std::vector<vec2d> &points) const
    {
        switch(macro.aperture_type) {

        case aperture_type_macro_circle: {
            double rotation = 0.0;
            if(macro.parameters.size() > circle_rotation) {
                rotation = macro.parameters[circle_rotation];
            }
            vec2d center{ macro.parameters[circle_centre_x], macro.parameters[circle_centre_y] };
            matrix apm = matrix::multiply(aperture_matrix, matrix::rotate(rotation));
            apm = matrix::multiply(apm, matrix::translate(net->end));
            center = vec2d(center, apm);
            double radius = macro.parameters[circle_diameter] / 2;
            points.clear();
            points.reserve(4);
            points.emplace_back(center.x - radius, center.y + radius);
            points.emplace_back(center.x + radius, center.y + radius);
            points.emplace_back(center.x - radius, center.y - radius);
            points.emplace_back(center.x + radius, center.y - radius);
        } break;

        case aperture_type_macro_moire: {
            double crosshair_length = macro.parameters[moire_crosshair_length];
            double outside_diameter = macro.parameters[moire_outside_diameter];
            vec2d offset{ macro.parameters[moire_centre_x], macro.parameters[moire_centre_y] };
            double radius = std::max(crosshair_length, outside_diameter) / 2.0;
            vec2d center{ net->end.x + offset.x, net->end.y + offset.y };
            points.clear();
            points.reserve(4);
            points.emplace_back(center.x - radius, center.y + radius, aperture_matrix);
            points.emplace_back(center.x + radius, center.y + radius, aperture_matrix);
            points.emplace_back(center.x - radius, center.y - radius, aperture_matrix);
            points.emplace_back(center.x + radius, center.y - radius, aperture_matrix);
        } break;

        case aperture_type_macro_thermal: {
            double radius = macro.parameters[thermal_outside_diameter] / 2.0;
            vec2d offset{ macro.parameters[thermal_centre_x], macro.parameters[thermal_centre_y] };
            vec2d center{ net->end.x + offset.x, net->end.y + offset.y };
            points.clear();
            points.reserve(4);
            points.emplace_back(center.x - radius, center.y + radius, aperture_matrix);
            points.emplace_back(center.x + radius, center.y + radius, aperture_matrix);
            points.emplace_back(center.x - radius, center.y - radius, aperture_matrix);
            points.emplace_back(center.x + radius, center.y - radius, aperture_matrix);
        } break;

        case aperture_type_macro_polygon: {
            size_t num_sides = static_cast<size_t>(macro.parameters[polygon_number_of_sides]);
            double diameter = macro.parameters[polygon_diameter];
            double rotation = macro.parameters[polygon_rotation];
            vec2d offset{ macro.parameters[polygon_centre_x], macro.parameters[polygon_centre_y] };
            matrix apm = matrix::multiply(aperture_matrix, matrix::rotate_around(rotation, offset));
            apm = matrix::multiply(apm, matrix::translate(net->end));
            points.reserve(num_sides);
            points.emplace_back(diameter / 2.0 + offset.x, offset.y, aperture_matrix);
            for(size_t i = 1; i < num_sides; ++i) {
                double angle = i * M_PI * 2.0 / num_sides;
                double x = cos(angle) * diameter / 2.0 + offset.x;
                double y = sin(angle) * diameter / 2.0 + offset.y;
                points.emplace_back(x, y, apm);
            }
        } break;

        case aperture_type_macro_outline: {
            size_t num_points = static_cast<size_t>(macro.parameters[outline_number_of_points]);
            vec2d offset{ macro.parameters[outline_first_x], macro.parameters[outline_first_x] };
            double rotation = macro.parameters[num_points * 2 + outline_rotation];
            matrix apm = matrix::multiply(aperture_matrix, matrix::rotate_around(rotation, offset));
            apm = matrix::multiply(apm, matrix::translate(net->end));
            points.clear();
            points.reserve(num_points + 1);
            for(size_t p = 0; p <= num_points; ++p) {
                double x = macro.parameters[p * 2 + outline_first_x];
                double y = macro.parameters[p * 2 + outline_first_y];
                points.emplace_back(x, y, apm);
            }
        } break;

        case aperture_type_macro_line20: {
            vec2d start{ macro.parameters[line_20_start_x], macro.parameters[line_20_start_y] };
            vec2d end{ macro.parameters[line_20_end_x], macro.parameters[line_20_end_y] };
            double width = macro.parameters[line_20_line_width] / 2.0;
            if(width != 0.0) {
                double rotation = macro.parameters[line_20_rotation];
                matrix apm = matrix::multiply(aperture_matrix, matrix::rotate_around(rotation, { net->end.x + start.x, net->end.y + start.y }));
                points.clear();
                points.reserve(4);
                points.emplace_back(net->end.x + start.x + width, net->end.y + start.y - width, apm);
                points.emplace_back(net->end.x + start.x + width, net->end.y + start.y + width, apm);
                points.emplace_back(net->end.x + end.x - width, net->end.y + end.y - width, apm);
                points.emplace_back(net->end.x + end.x - width, net->end.y + end.y + width, apm);
            }
        } break;

        case aperture_type_macro_line21: {
            double rotation = macro.parameters[line_21_rotation];
            double width = macro.parameters[line_21_line_width] / 2.0;
            double height = macro.parameters[line_21_line_height] / 2.0;
            if(width != 0.0 && height != 0.0) {
                vec2d offset{ macro.parameters[line_21_centre_x], macro.parameters[line_21_centre_y] };
                matrix apm = matrix::multiply(aperture_matrix, matrix::rotate_around(rotation, offset));
                apm = matrix::multiply(apm, matrix::translate(net->end));
                points.clear();
                points.reserve(4);
                points.emplace_back(-width, -height, apm);
                points.emplace_back(+width, -height, apm);
                points.emplace_back(+width, +height, apm);
                points.emplace_back(-width, +height, apm);
            }
        } break;

        case aperture_type_macro_line22: {
            double rotation = macro.parameters[line_22_rotation];
            double width = macro.parameters[line_22_line_width];
            double height = macro.parameters[line_22_line_height];
            if(width != 0.0 && height != 0.0) {
                vec2d offset{ macro.parameters[line_22_lower_left_x], macro.parameters[line_22_lower_left_y] };
                matrix apm = matrix::multiply(aperture_matrix, matrix::rotate_around(rotation, offset));
                double llx = net->end.x + offset.x;
                double lly = net->end.y + offset.y;
                points.clear();
                points.reserve(4);
                points.emplace_back(llx, lly, apm);
                points.emplace_back(llx + width, lly, apm);
                points.emplace_back(llx + width, lly + height, apm);
                points.emplace_back(llx, lly + height, apm);
            }
        } break;

        default:
            return error_invalid_aperture_type;
        }

        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_entity &gerber_file::add_entity()
    {
        entities.emplace_back(reader.line_number, reader.line_number, image.nets.size());
        gerber_entity &e = entities.back();
        e.attributes = attributes;
        return e;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::parse_gerber_segment(gerber_net *net)
    {
        LOG_CONTEXT("parse_segment", debug);

        rect whole_box{ DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX };

        rect bounding_box = whole_box;

        double unit_scale;

        int coordinate{};

        vec2d scale;
        vec2d center;

        int region_points{};

        bool done{ false };

        int entity_id = 0;

        while(!reader.eof() && !done) {

            if(state.net_state->unit == unit_millimeter) {
                unit_scale = 1.0;
            } else {
                unit_scale = 1.0 / 25.4;
            }

            char c;
            gerber_error_code err = reader.read_char(&c);
            if(err == error_end_of_file) {
                break;
            }
            CHECK(err);

            switch(c) {

            case 'G': {
                CHECK(parse_g_code());
            } break;

            case 'D': {
                CHECK(parse_d_code());
            } break;

            case 'M': {
                done = parse_m_code();
            } break;

            case 'X': {
                stats.x_count += 1;
                size_t length = 0;
                CHECK(reader.get_int(&coordinate, &length));
                if(image.format.omit_zeros == omit_zeros_trailing) {
                    add_trailing_zeros(image.format.integral_part_x, image.format.decimal_part_x, static_cast<int>(length), &coordinate);
                }
                if(image.format.coordinate == coordinate_incremental) {
                    if(coordinate != 0) {
                        state.current_x += coordinate;
                        state.changed();
                    }
                } else {
                    if(state.current_x != coordinate) {
                        state.current_x = coordinate;
                        state.changed();
                    }
                }
            } break;

            case 'Y': {
                stats.y_count += 1;
                size_t length = 0;
                CHECK(reader.get_int(&coordinate, &length));
                if(image.format.omit_zeros == omit_zeros_trailing) {
                    add_trailing_zeros(image.format.integral_part_x, image.format.decimal_part_y, static_cast<int>(length), &coordinate);
                }
                if(image.format.coordinate == coordinate_incremental) {
                    if(coordinate != 0) {
                        state.current_y += coordinate;
                        state.changed();
                    }
                } else {
                    if(state.current_y != coordinate) {
                        state.current_y = coordinate;
                        state.changed();
                    }
                }
                // LOG_DEBUG("Y: X = {}, Y = {}", state.current_x, state.current_y);
            } break;

            case 'I': {
                stats.i_count += 1;
                size_t length = 0;
                CHECK(reader.get_int(&coordinate, &length));
                if(image.format.omit_zeros == omit_zeros_trailing) {
                    add_trailing_zeros(image.format.integral_part_x, image.format.decimal_part_x, static_cast<int>(length), &coordinate);
                }
                state.center_x = coordinate;
                // LOG_DEBUG("CX = {}", state.center_x);
                state.changed();
            } break;

            case 'J': {
                stats.j_count += 1;
                size_t length = 0;
                CHECK(reader.get_int(&coordinate, &length));
                if(image.format.omit_zeros == omit_zeros_trailing) {
                    add_trailing_zeros(image.format.integral_part_y, image.format.decimal_part_y, static_cast<int>(length), &coordinate);
                }
                state.center_y = coordinate;
                // LOG_DEBUG("CY = {}", state.center_y);
                state.changed();
            } break;

            case '%': {
                CHECK(parse_rs274x(net));
            } break;

            case '*': {

                stats.star_count += 1;
                if(!state.changed_state) {
                    break;
                }
                state.changed(false);

                // Don't even bother saving the geberNet if the aperture state is GERBER_APERTURE_STATE_OFF and we
                // aren't starting a polygon fill (where we need it to get to the start point)

                if(state.aperture_state == aperture_state_off && !state.is_region_fill && state.interpolation != interpolation_region_start) {

                    // Save the coordinate so the next Net can use it for a start point
                    state.previous_x = state.current_x;
                    state.previous_y = state.current_y;
                    break;
                }

                // round to nearest N decimal places

                auto round_to = [](double D, int N) {
                    double m = pow(10.0, N);
                    return round(D * m) / m;
                };

                // Entity detection

                int current_entity_id = entity_id;

                if(state.is_region_fill) {
                    if(state.interpolation == interpolation_region_end) {
                        if(entities.empty()) {
                            LOG_ERROR("Huh? Wheres the entity man?");
                            add_entity();
                        }
                        entities.back().line_number_end = reader.line_number;
                        entity_id += 1;
                        LOG_VERBOSE("ENTITY {} ENDS: {}", entity_id, entities.back());
                    }
                } else
                    switch(state.aperture_state) {

                    case aperture_state_off:
                    case aperture_state_on:

                        switch(state.interpolation) {
                        case interpolation_linear:
                        case interpolation_clockwise_circular:
                        case interpolation_counterclockwise_circular:
                            add_entity();
                            LOG_VERBOSE("ENTITY {} OCCURS: {} ({})", entity_id, entities.back(), state.aperture_state);
                            entity_id += 1;
                            break;
                        case interpolation_region_start:
                            add_entity();
                            LOG_VERBOSE("ENTITY {} OCCURS: {} ({})", entity_id, entities.back(), state.aperture_state);
                            break;
                        case interpolation_region_end:
                            LOG_ERROR("Shouldn't get here...({})", state.aperture_state);
                            break;
                        }
                        break;

                    case aperture_state_flash:
                        add_entity();
                        LOG_VERBOSE("ENTITY {} OCCURS: {} ({})", entity_id, entities.back(), state.aperture_state);
                        entity_id += 1;
                        break;
                    }

                net = new gerber_net(&image, net, state.level, state.net_state);
                net->entity_id = current_entity_id;

                scale.x = pow(10.0, image.format.decimal_part_x) * unit_scale;
                scale.y = pow(10.0, image.format.decimal_part_y) * unit_scale;

                net->start.x = state.previous_x / scale.x;
                net->start.y = state.previous_y / scale.y;
                net->end.x = state.current_x / scale.x;
                net->end.y = state.current_y / scale.y;
                center.x = state.center_x / scale.x;
                center.y = state.center_y / scale.y;

                net->start.x = round_to(net->start.x, accuracy_decimal_places);
                net->start.y = round_to(net->start.y, accuracy_decimal_places);
                net->end.x = round_to(net->end.x, accuracy_decimal_places);
                net->end.y = round_to(net->end.y, accuracy_decimal_places);
                center.x = round_to(center.x, accuracy_decimal_places);
                center.y = round_to(center.y, accuracy_decimal_places);

                if(!state.is_region_fill) {
                    bounding_box = whole_box;
                }

                switch(state.interpolation) {

                case interpolation_clockwise_circular:
                case interpolation_counterclockwise_circular: {
                    bool cw = state.interpolation == interpolation_clockwise_circular;
                    CHECK(calculate_arc(net, state.is_multi_quadrant, cw, center));
                } break;

                case interpolation_region_start: {
                    state.aperture_state = aperture_state_on;    // Aperure state set to on for polygon areas.
                    state.region_start_node = net;               // To be able to get back and fill in number of polygon corners.
                    state.is_region_fill = true;
                    state.current_aperture = 0;
                    region_points = 0;
                    bounding_box = whole_box;
                } break;

                case interpolation_region_end: {
                    state.region_start_node->bounding_box = bounding_box;    // Save the calculated bounding box to the start node.
                    state.region_start_node->num_region_points = region_points;
                    state.region_start_node = nullptr;
                    state.is_region_fill = false;
                    region_points = 0;
                    update_image_bounds(bounding_box, 0, 0, image);
                    bounding_box = whole_box;
                } break;

                case interpolation_linear:
                    if(state.is_region_fill) {
                        update_bounds(bounding_box, matrix::identity(), net->end);
                    }
                    break;

                default:
                    // not arc or region start/end
                    break;
                }

                if(state.is_region_fill && state.region_start_node != nullptr) {

                    // "...all lines drawn with D01 are considered edges of the
                    // polygon. D02 closes and fills the polygon."
                    // p.49 rs274xrevd_e.pdf
                    // D02 . state.apertureState == GERBER_APERTURE_STATE_OFF

                    // UPDATE: only end the region during a D02 call if we've already
                    // drawn a polygon edge (with D01)

                    if(state.aperture_state == aperture_state_off && state.interpolation != interpolation_region_start && region_points > 0) {

                        net->interpolation_method = interpolation_region_end;
                        state.region_start_node->num_region_points = region_points;

                        net = new gerber_net(&image, net, state.level, state.net_state);
                        net->entity_id = current_entity_id;
                        net->interpolation_method = interpolation_region_start;
                        state.region_start_node->bounding_box = bounding_box;
                        state.region_start_node = net;
                        region_points = 0;

                        net = new gerber_net(&image, net, state.level, state.net_state);
                        net->entity_id = current_entity_id;
                        net->start.x = state.previous_x / scale.x;
                        net->start.y = state.previous_y / scale.y;
                        net->end.x = state.current_x / scale.x;
                        net->end.y = state.current_y / scale.y;

                        net->start.x = round_to(net->start.x, accuracy_decimal_places);
                        net->start.y = round_to(net->start.y, accuracy_decimal_places);
                        net->end.x = round_to(net->end.x, accuracy_decimal_places);
                        net->end.y = round_to(net->end.y, accuracy_decimal_places);

                    } else if(state.interpolation != interpolation_region_start) {
                        region_points += 1;
                    }
                }
                net->interpolation_method = state.interpolation;

                // Override circular interpolation if no center was given.
                // This should be a safe hack, since a good file should always
                // include I or J. And even if the radius is zero, the end point
                // should be the same as the start point, creating no line

                if((state.interpolation == interpolation_clockwise_circular || state.interpolation == interpolation_counterclockwise_circular) &&
                   state.center_x == 0.0 && state.center_y == 0.0) {

                    net->interpolation_method = interpolation_linear;
                }

                // If we detected the end of a region we go back to
                // the interpolation we had before that.
                // Also if we detected any of the quadrant flags, since some
                // gerbers don't reset the interpolation (EagleCad again).

                if(state.interpolation == interpolation_region_start || state.interpolation == interpolation_region_end) {

                    state.interpolation = state.previous_interpolation;
                }

                // Save level polarity and unit
                net->level = state.level;

                state.center_x = 0;
                state.center_y = 0;

                net->aperture = state.current_aperture;
                net->aperture_state = state.aperture_state;

                // For next round we save the current position as the previous position
                state.previous_x = state.current_x;
                state.previous_y = state.current_y;

                // If we have an aperture defined at the moment we find min and max of image with compensation for mm. else we're done
                if(net->aperture == 0 && !state.is_region_fill) {
                    break;
                }
                // Only update the min/max values and aperture stats if we are drawing.
                aperture_matrix = matrix::identity();

                if(net->aperture_state != aperture_state_off && net->interpolation_method != interpolation_region_start) {

                    vec2d repeat_offset{};

                    if(!state.is_region_fill) {

                        gerber_error_code error = stats.increment_d_list_count(net->aperture, 1, reader.line_number);

                        if(error != ok) {
                            net->aperture_state = aperture_state_off;
                            stats.error(reader, error, "undefined: D{}", net->aperture);
                            stats.unknown_d_codes += 1;
                        }

                        //  If step_and_repeat (%SR%) is used, check min_x, max_y etc for
                        //  the ends of the step_and_repeat lattice. This goes wrong in
                        //  the case of negative dist_X or dist_Y, in which case we
                        //  should compare against the start points of the lines, not
                        //  the stop points, but that seems an uncommon case (and the
                        //  error isn't very big any way).

                        repeat_offset.x = (state.level->step_and_repeat.pos.x - 1) * state.level->step_and_repeat.distance.x;
                        repeat_offset.y = (state.level->step_and_repeat.pos.y - 1) * state.level->step_and_repeat.distance.y;

                        aperture_matrix = matrix::multiply(matrix::translate({ image.info.offset_a, image.info.offset_b }), aperture_matrix);
                        aperture_matrix = matrix::multiply(matrix::rotate(image.info.image_rotation), aperture_matrix);
                        aperture_matrix = matrix::multiply(matrix::translate(state.net_state->offset), aperture_matrix);

                        // Apply mirror.
                        switch(state.net_state->mirror_state) {

                        case mirror_state_flip_a:
                            aperture_matrix = matrix::multiply(matrix::scale({ -1, 1 }), aperture_matrix);
                            break;

                        case mirror_state_flip_b:
                            aperture_matrix = matrix::multiply(matrix::scale({ 1, -1 }), aperture_matrix);
                            break;

                        case mirror_state_flip_ab:
                            aperture_matrix = matrix::multiply(matrix::scale({ -1, -1 }), aperture_matrix);
                            break;

                        default:
                            break;
                        }

                        // Apply axis select
                        if(state.net_state->axis_select == axis_select_swap_ab) {

                            // do this by rotating 90 clockwise, then mirroring the Y axis.
                            aperture_matrix = matrix::multiply(matrix::rotate(90.0), aperture_matrix);
                            aperture_matrix = matrix::multiply(matrix::scale({ 1, -1 }), aperture_matrix);
                        }

                        gerber_aperture *a = nullptr;
                        auto ap = image.apertures.find(net->aperture);
                        if(ap != image.apertures.end()) {
                            a = ap->second;
                        }

                        if(a != nullptr && a->aperture_type == aperture_type_macro) {
                            bounding_box = whole_box;
                            std::vector<vec2d> points{};
                            for(auto m : a->macro_parameters_list) {
                                CHECK(get_aperture_points(*m, net, points));
                                update_net_bounds(bounding_box, points);
                            }
                            update_image_bounds(bounding_box, repeat_offset.x, repeat_offset.y, image);
                        } else {

                            vec2d ap_size{};

                            if(a != nullptr) {
                                ap_size.x = a->parameters[0];
                                ap_size.y = ap_size.x;
                                if(a->aperture_type == aperture_type_rectangle || a->aperture_type == aperture_type_oval) {
                                    ap_size.y = a->parameters[1];
                                }
                            }
                            // If it's an arc path, use a special calculation.
                            if(net->interpolation_method == interpolation_clockwise_circular ||
                               net->interpolation_method == interpolation_counterclockwise_circular) {

                                // BUT.... matrix transform...

                                gerber_arc const &arc = net->circle_segment;

                                rect arc_extent = get_arc_extents(arc.pos, arc.size.x / 2, arc.start_angle, arc.end_angle);

                                vec2d ha{ ap_size.x / 2, ap_size.y / 2 };
                                arc_extent.min_pos = arc_extent.min_pos.subtract(ha);
                                arc_extent.max_pos = arc_extent.max_pos.add(ha);

                                update_bounds(bounding_box, aperture_matrix, arc_extent.min_pos);
                                update_bounds(bounding_box, aperture_matrix, arc_extent.max_pos);

                            } else {

                                // Check both the start and stop of the aperture points against a running min/max counter
                                // Note: only check start coordinate if this isn't a flash,
                                // since the start point may be invalid if it is a flash.
                                if(net->aperture_state != aperture_state_flash) {
                                    // Start points.
                                    update_net_bounds(bounding_box, net->start.x, net->start.y, ap_size.x / 2.0, ap_size.y / 2.0);
                                }

                                // Stop points.
                                update_net_bounds(bounding_box, net->end.x, net->end.y, ap_size.x / 2, ap_size.y / 2);
                            }
                            // Update the info bounding box with this latest bounding box
                            // don't change the bounding box if the polarity is clear or negative
                            if(state.level->polarity != polarity_clear) {
                                update_image_bounds(bounding_box, repeat_offset.x, repeat_offset.y, image);
                            }

                            // Optionally update the knockout measurement box.
                            if(knockout_measure) {

                                if(bounding_box.min_pos.x < knockout_limit_min.x) {
                                    knockout_limit_min.x = bounding_box.min_pos.x;
                                }

                                if(bounding_box.max_pos.y < knockout_limit_min.y) {
                                    knockout_limit_min.y = bounding_box.max_pos.y;
                                }

                                if(bounding_box.max_pos.x + repeat_offset.x > knockout_limit_max.x) {
                                    knockout_limit_max.x = bounding_box.max_pos.x + repeat_offset.x;
                                }

                                if(bounding_box.min_pos.y + repeat_offset.y > knockout_limit_max.y) {
                                    knockout_limit_max.y = bounding_box.min_pos.y + repeat_offset.y;
                                }
                            }

                            // If we're not in a polygon fill, then update the current object bounding box
                            // and instansiate a new one for the next net.
                            if(!state.is_region_fill) {
                                net->bounding_box = bounding_box;
                            }
                        }
                    }
                }
            } break;
            }
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::fill_region_path(gerber_draw_interface &drawer, size_t net_index, gerber_polarity polarity) const
    {
        std::vector<gerber_draw_element> elements;

        gerber_net *gnet = image.nets[net_index];

        for(size_t last_index = net_index + 1; last_index < image.nets.size(); ++last_index) {

            gerber_net *n = image.nets[last_index];

            if(n->interpolation_method == interpolation_region_end) {
                break;
            }

            if(n->aperture_state == aperture_state_on) {

                switch(n->interpolation_method) {

                case interpolation_linear: {
                    if(n->start.x != n->end.x || n->start.y != n->end.y) {
                        elements.emplace_back(n->start, n->end);
                        //} else {
                        //    LOG_DEBUG("IGNORED empty edge at index {} (line {})", last_index, n->line_number);
                    }
                } break;

                case interpolation_clockwise_circular:
                case interpolation_counterclockwise_circular: {
                    double a1 = n->circle_segment.start_angle;
                    double a2 = n->circle_segment.end_angle;
                    elements.emplace_back(n->circle_segment.pos, a1, a2, n->circle_segment.size.x / 2);
                } break;

                default:
                    LOG_ERROR("Huh? Bogus interpolation method {} ({}) in outline", static_cast<int>(n->interpolation_method), n->interpolation_method);
                    return error_invalid_interpolation;
                }
            }
        }
        return drawer.fill_elements(elements.data(), elements.size(), polarity, gnet);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::fill_polygon(gerber_draw_interface &drawer, double diameter, int num_sides, double angle_degrees) const
    {
        LOG_DEBUG("fill_polygon");
        (void)drawer;
        (void)diameter;
        (void)num_sides;
        (void)angle_degrees;
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_macro(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *const macro_aperture) const
    {
        for(auto m : macro_aperture->macro_parameters_list) {

            switch(m->aperture_type) {

            case aperture_type_macro_circle: {
                FAIL_IF(m->parameters.size() < circle_num_parameters - 1llu, error_bad_parameter_count);
                double exposure = m->parameters[circle_exposure];
                double diameter = m->parameters[circle_diameter];

                double rotation = 0;
                if(m->parameters.size() == circle_num_parameters) {
                    rotation = m->parameters[circle_rotation];
                }
                gerber_polarity polarity{ polarity_dark };
                if(exposure < 0.01) {
                    polarity = polarity_clear;
                }
                vec2d pos(m->parameters[circle_centre_x], m->parameters[circle_centre_y]);
                matrix mat = matrix::rotate(rotation);
                mat = matrix::multiply(mat, matrix::translate(net->end));
                pos = transform_point(mat, pos);
                gerber_draw_element e(pos, 0, 360, diameter / 2);
                return drawer.fill_elements(&e, 1, polarity, net);
            }

            case aperture_type_macro_moire: {
                FAIL_IF(m->parameters.size() < moire_num_parameters, error_bad_parameter_count);
                // moire
            } break;

            case aperture_type_macro_thermal: {
                FAIL_IF(m->parameters.size() < thermal_num_parameters, error_bad_parameter_count);
                // thermal
            } break;

            case aperture_type_macro_outline: {
                FAIL_IF(m->parameters.size() < outline_num_parameters, error_bad_parameter_count);
                std::vector<vec2d> points;
                CHECK(get_aperture_points(*m, net, points));
                std::vector<gerber_draw_element> e;
                for(size_t i = 0; i < points.size() - 1; ++i) {
                    e.emplace_back(points[i], points[i + 1]);
                }
                return drawer.fill_elements(e.data(), e.size(), polarity_dark, net);
            }

            case aperture_type_macro_polygon: {
                FAIL_IF(m->parameters.size() < polygon_num_parameters, error_bad_parameter_count);
            } break;

            case aperture_type_macro_line20: {
                FAIL_IF(m->parameters.size() < line_20_num_parameters, error_bad_parameter_count);
                vec2d start{ m->parameters[line_20_start_x], m->parameters[line_20_start_y] };
                vec2d end{ m->parameters[line_20_end_x], m->parameters[line_20_end_y] };
                double width = m->parameters[line_20_line_width];
                if(width == 0) {
                    return error_invalid_number;
                }
                double w2 = width / 2;
                double rotation = m->parameters[line_20_rotation];

                matrix mat = matrix::rotate(rotation);
                mat = matrix::multiply(mat, matrix::translate(net->end));
                // transform_points(mat, points);

                std::array points = { vec2d{ start.x, start.y - w2, mat },    //
                                      vec2d{ end.x, start.y - w2, mat },      //
                                      vec2d{ end.x, start.y + w2, mat },      //
                                      vec2d{ start.x, start.y + w2, mat } };


                gerber_draw_element e[4];
                e[0] = gerber_draw_element(points[0], points[3]);
                e[1] = gerber_draw_element(points[3], points[2]);
                e[2] = gerber_draw_element(points[2], points[1]);
                e[3] = gerber_draw_element(points[1], points[0]);
                return drawer.fill_elements(e, 4, polarity_dark, net);
            }

            case aperture_type_macro_line21: {
                FAIL_IF(m->parameters.size() < line_21_num_parameters, error_bad_parameter_count);
                double x = m->parameters[line_21_centre_x];
                double y = m->parameters[line_21_centre_y];
                double w = m->parameters[line_21_line_width];
                double h = m->parameters[line_21_line_height];
                if(w == 0 || h == 0) {
                    return error_invalid_number;
                }

                double rotation = m->parameters[line_21_rotation];
                double w2 = w / 2;
                double h2 = h / 2;

                matrix mat = matrix::rotate(rotation);
                mat = matrix::multiply(mat, matrix::translate(net->end));

                std::array points = { vec2d({ x - w2, y - h2 }, mat),      //
                                      vec2d({ x + w2, y - h2 }, mat),      //
                                      vec2d({ x + w2, y + h2 }, mat),      //
                                      vec2d({ x - w2, y + h2 }, mat) };    //

                gerber_draw_element e[4];
                e[0] = gerber_draw_element(points[0], points[3]);
                e[1] = gerber_draw_element(points[3], points[2]);
                e[2] = gerber_draw_element(points[2], points[1]);
                e[3] = gerber_draw_element(points[1], points[0]);
                return drawer.fill_elements(e, 4, polarity_dark, net);
            }

            case aperture_type_macro_line22: {
                FAIL_IF(m->parameters.size() < line_22_num_parameters, error_bad_parameter_count);
            } break;
            default:
                LOG_FATAL("Bad aperture type: {}", m->aperture_type);
                break;
            }
        }
        return error_invalid_aperture_type;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_linear_circle(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const
    {
        double width = aperture->parameters[0];

        vec2d start = net->start;
        vec2d end = net->end;

        double thickness = width / 2;

        if(start.x == end.x && start.y == end.y) {
            return draw_circle(drawer, net, net->start, thickness);
        }

        double dx = end.x - start.x;
        double dy = end.y - start.y;

        double rad = atan2(dy, dx);

        double ox = cos(rad) * thickness;
        double oy = sin(rad) * thickness;

        double deg = rad_2_deg(rad);

        vec2d p1(start.x + oy, start.y - ox);
        vec2d p2(end.x + oy, end.y - ox);
        vec2d p3(end.x - oy, end.y + ox);
        vec2d p4(start.x - oy, start.y + ox);

        gerber_draw_element e[4] = { gerber_draw_element(p2, p1),                                     //
                                     gerber_draw_element(end, deg - 90, deg + 90, thickness),         //
                                     gerber_draw_element(p4, p3),                                     //
                                     gerber_draw_element(start, deg + 90, deg + 270, thickness) };    //

        return drawer.fill_elements(e, 4, net->level->polarity, net);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_linear_rectangle(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const
    {
        double w = aperture->parameters[0] / 2;
        double h = aperture->parameters[1] / 2;
        vec2d size{ w, h };
        vec2d start = net->start;
        vec2d end = net->end;
        vec2d diff = end.subtract(start);

        auto draw_rectangle = [&](vec2d const &bottom_left, vec2d const &top_right) {
            gerber_draw_element el[4];
            el[0] = gerber_draw_element(bottom_left, { top_right.x, bottom_left.y });
            el[1] = gerber_draw_element(el[0].line.end, top_right);
            el[2] = gerber_draw_element(el[1].line.end, { bottom_left.x, top_right.y });
            el[3] = gerber_draw_element(el[2].line.end, el[0].line.start);
            return drawer.fill_elements(el, 4, net->level->polarity, net);
        };

        if(diff.length() < 1e-6) {
            // just draw a rectangle at start pos
            return draw_rectangle(start.subtract(size), start.add(size));
        }
        if(start.x == end.x) {
            // draw vertical
            return draw_rectangle({ start.x - w, std::min(start.y, end.y) - h }, { start.x + w, std::max(start.y, end.y) + h });
        }
        if(start.y == end.y) {
            // draw horizontal
            return draw_rectangle({ std::min(start.x, end.x) - w, start.y - h }, { std::max(start.x, end.x) + w, start.y + h });
        }
        // draw 6 sided shape
        LOG_ERROR("Say wha?");
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_linear_interpolation(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const
    {
        switch(aperture->aperture_type) {
        case aperture_type_circle:
            draw_linear_circle(drawer, net, aperture);
            break;
        case aperture_type_rectangle:
            draw_linear_rectangle(drawer, net, aperture);
            break;
        default:
            LOG_WARNING("linear interpolation for aperture {} not supported", aperture->aperture_type);
            break;
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_circle(gerber_draw_interface &drawer, gerber_net *net, vec2d const &pos, double radius) const
    {
        gerber_draw_element e(pos, 0.0, 360.0, radius);
        return drawer.fill_elements(&e, 1, net->level->polarity, net);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_arc(gerber_draw_interface &drawer, gerber_net *net, double thickness) const
    {
        gerber_arc const &arc = net->circle_segment;

        vec2d const &pos = arc.pos;
        double start_angle = arc.start_angle;
        double end_angle = arc.end_angle;
        double radius = arc.size.x / 2;

        double r = thickness / 2;
        double inner_radius = radius - r;
        double outer_radius = radius + r;

        gerber_draw_element draw_elements[7];
        int n = 0;

        auto add_arc = [&](vec2d const &_pos, double _radius, double s, double e) {
            draw_elements[n] = gerber_draw_element(_pos, s, e, _radius);
            n += 1;
        };

        auto add_line = [&](vec2d const &a, vec2d const &b) {
            draw_elements[n] = gerber_draw_element(a, b);
            n += 1;
        };

        // this is nasty, there are a few cases

        if(start_angle > end_angle) {
            // hmmm
        } else if(start_angle == end_angle) {
            // just a point of thickness
            double start_radians = deg_2_rad(start_angle);
            vec2d start_pos{ pos.x + cos(start_radians) * radius, pos.y + sin(start_radians) * radius };
            add_arc(start_pos, r, 0, 360);
        } else if(end_angle - start_angle >= 360) {

            // a ring of thickness
            add_arc(pos, outer_radius, 0, 360);

            // might be just a circle if it's too thick to show the middle bit
            if(inner_radius > 0) {
                add_line({ pos.x + outer_radius, pos.y }, { pos.x + inner_radius, pos.y });
                add_arc(pos, inner_radius, 360, 0);
            }

        } else {

            // ok, it's an arc, but do the ends overlap?
            double start_radians = deg_2_rad(start_angle);
            double end_radians = deg_2_rad(end_angle);

            vec2d start_pos{ pos.x + cos(start_radians) * radius, pos.y + sin(start_radians) * radius };
            vec2d end_pos{ pos.x + cos(end_radians) * radius, pos.y + sin(end_radians) * radius };

            double l_squared = start_pos.subtract(end_pos).length_squared();

            bool overlap = l_squared < thickness * thickness && end_angle - start_angle > 180;

            if(!overlap) {

                // no overlap, just draw the inner, outer and end caps
                add_arc(pos, outer_radius, start_angle, end_angle);
                add_arc(end_pos, r, end_angle, end_angle + 180);
                add_arc(pos, inner_radius, end_angle, start_angle);
                add_arc(start_pos, r, start_angle - 180, start_angle);

            } else {

                // ends overlap, calculate the niggly little bits
                double mid_angle = (start_radians + end_radians) / 2;

                double mid_angle_90 = mid_angle + M_PI / 2;
                double theta = acos(sqrt(l_squared) / 2 / r);

                double inner_angle = mid_angle_90 - theta;
                double outer_angle = mid_angle_90 + theta;

                vec2d inner_intersect{ start_pos.x + cos(inner_angle) * r, start_pos.y + sin(inner_angle) * r };
                vec2d outer_intersect{ start_pos.x + cos(outer_angle) * r, start_pos.y + sin(outer_angle) * r };

                double inner_degrees = rad_2_deg(inner_angle);
                double outer_degrees = rad_2_deg(outer_angle);

                add_arc(start_pos, r, outer_degrees, start_angle + 360);
                add_arc(pos, outer_radius, start_angle, end_angle);
                add_arc(end_pos, r, end_angle, inner_degrees + 180);

                add_line(inner_intersect, outer_intersect);

                // only draw the inner bit if there's room for it
                if(radius > r) {

                    add_arc(end_pos, r, outer_degrees + 180, end_angle + 180);
                    add_arc(pos, inner_radius, end_angle, start_angle);
                    add_arc(start_pos, r, start_angle + 180, inner_degrees);
                }
            }
        }
        if(n != 0) {
            return drawer.fill_elements(draw_elements, n, net->level->polarity, net);
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_capsule(gerber_draw_interface &drawer, gerber_net *net, double width, double height) const
    {
        vec2d const &center = net->end;
        gerber_draw_element el[4];

        double w2 = width / 2;
        double h2 = height / 2;

        vec2d tl1{ center.x - w2, center.y - h2 };
        vec2d br1{ center.x + w2, center.y + h2 };

        if(fabs(width - height) < 1e-6) {
            return draw_circle(drawer, net, center, w2);
        }
        if(width > height) {
            vec2d tl2{ tl1.x + h2, tl1.y };
            vec2d br2{ br1.x - h2, br1.y };
            el[0] = gerber_draw_element({ tl2.x, center.y }, 90, 270, h2);
            el[1] = gerber_draw_element(tl2, { br2.x, tl1.y });
            el[2] = gerber_draw_element({ br2.x, center.y }, 270, 450, h2);
            el[3] = gerber_draw_element(br2, { tl2.x, br1.y });
            return drawer.fill_elements(el, 4, net->level->polarity, net);
        }
        vec2d tl2{ tl1.x, tl1.y + w2 };
        vec2d br2{ br1.x, br1.y - w2 };
        el[0] = gerber_draw_element({ center.x, tl2.y }, 180, 360, w2);
        el[1] = gerber_draw_element({ br2.x, tl2.y }, br2);
        el[2] = gerber_draw_element({ center.x, br2.y }, 0, 180, w2);
        el[3] = gerber_draw_element({ tl2.x, br2.y }, tl2);
        return drawer.fill_elements(el, 4, net->level->polarity, net);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw_rectangle(gerber_draw_interface &drawer, gerber_net *net, rect const &draw_rect) const
    {
        rect r = { draw_rect.min_pos.add(net->end), draw_rect.max_pos.add(net->end) };
        vec2d bottom_right = vec2d{ r.max_pos.x, r.min_pos.y };
        vec2d top_left = vec2d{ r.min_pos.x, r.max_pos.y };
        gerber_draw_element el[4];
        el[0] = gerber_draw_element(r.min_pos, bottom_right);
        el[1] = gerber_draw_element(bottom_right, r.max_pos);
        el[2] = gerber_draw_element(r.max_pos, top_left);
        el[3] = gerber_draw_element(top_left, r.min_pos);
        return drawer.fill_elements(el, 4, net->level->polarity, net);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::lines(gerber_draw_interface &drawer) const
    {
        // to skip a region block

        auto next_net_index = [this](size_t cur_index) {
            gerber_net *n = image.nets[cur_index];
            if(n->interpolation_method == interpolation_region_start) {
                while(cur_index < image.nets.size()) {
                    if(image.nets[cur_index]->interpolation_method == interpolation_region_end) {
                        break;
                    }
                    cur_index += 1;
                }
            }
            return cur_index + 1;
        };

        for(size_t net_index = 0; net_index < image.nets.size(); net_index = next_net_index(net_index)) {

            gerber_net *net = image.nets[net_index];

            if(net->level == nullptr) {
                continue;
            }

            if(net->hidden) {
                continue;
            }

            if(net->aperture_state == aperture_state_off) {
                continue;
            }

            gerber_aperture *aperture{ nullptr };
            map_get_if_found(image.apertures, net->aperture, &aperture);

            // LOG_DEBUG("Interpolation: {}", n->interpolation_method);

            switch(net->interpolation_method) {

            // draw the region
            case interpolation_region_start: {

                CHECK(fill_region_path(drawer, net_index, net->level->polarity));

            } break;

            default:

                if(aperture != nullptr) {

                    // LOG_DEBUG("Aperture type: {}, aperture state: {}", aperture->aperture_type, n->aperture_state);

                    switch(net->aperture_state) {

                    case aperture_state_off:
                        break;

                    case aperture_state_flash:
                        break;

                    // interpolate the aperture
                    case aperture_state_on:

                        switch(net->interpolation_method) {

                        // straight line
                        case interpolation_linear:
                            if(aperture->parameters.size() < 1) {
                                LOG_ERROR("Missing parameters for linear interpolation!?");
                            } else {
                                if(aperture->aperture_type != aperture_type_circle) {
                                    // LOG_DEBUG("{}", aperture->aperture_type);
                                }
                                CHECK(draw_linear_interpolation(drawer, net, aperture));
                            }
                            break;

                            // arc

                        case interpolation_clockwise_circular:
                        case interpolation_counterclockwise_circular:
                            if(aperture->parameters.size() < 1) {
                                LOG_ERROR("Missing parameters for arc!?");
                            } else {
                                CHECK(draw_arc(drawer, net, aperture->parameters[0]));
                            }
                            break;

                        default:
                            break;
                        }
                        break;
                    }
                    break;
                }
            }
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_file::draw(gerber_draw_interface &drawer) const
    {
        auto should_hide = [=](gerber_hide_elements h) { return (static_cast<int>(h) & hide_elements) != 0; };

        // to skip a region block

        auto next_net_index = [this](size_t cur_index) {
            gerber_net *n = image.nets[cur_index];
            if(n->interpolation_method == interpolation_region_start) {
                while(cur_index < image.nets.size()) {
                    if(image.nets[cur_index]->interpolation_method == interpolation_region_end) {
                        break;
                    }
                    cur_index += 1;
                }
            }
            return cur_index + 1;
        };

        size_t num_nets = image.nets.size();
        double percent = 0;

        gerber_timer timer;
        gerber_timer interim_timer;

        if(drawer.show_progress) {
            LOG_VERBOSE("DRAW BEGINS, {} nets", num_nets);
            timer.reset();
            interim_timer.reset();
        }

        for(size_t net_index = 0; net_index < image.nets.size(); net_index = next_net_index(net_index)) {

            if(drawer.show_progress) {
                double new_percent = net_index * 100.0 / num_nets;
                if(new_percent - percent >= 1 || interim_timer.elapsed_seconds() > 1) {
                    LOG_VERBOSE("{:5.2f}% ({} of {} nets)", new_percent, net_index, num_nets);
                    percent = new_percent;
                    interim_timer.reset();
                }
            }

            gerber_net *net = image.nets[net_index];

            if(net->level == nullptr) {
                LOG_ERROR("NO LEVEL for net at index {}!?", net_index);
                continue;
            }

            if(net->hidden) {
                continue;
            }

            if(net->aperture_state == aperture_state_off) {
                continue;
            }

            gerber_aperture *aperture{ nullptr };
            map_get_if_found(image.apertures, net->aperture, &aperture);

            // LOG_DEBUG("Interpolation: {}", n->interpolation_method);

            switch(net->interpolation_method) {

            // draw the region
            case interpolation_region_start: {

                if(!should_hide(hide_element_outlines)) {
                    CHECK(fill_region_path(drawer, net_index, net->level->polarity));
                }

            } break;

            default:

                if(aperture != nullptr) {

                    // LOG_DEBUG("Aperture type: {}, aperture state: {}", aperture->aperture_type, n->aperture_state);

                    switch(net->aperture_state) {

                    case aperture_state_off:
                        break;

                    // flash the aperture
                    case aperture_state_flash: {

                        switch(aperture->aperture_type) {

                        case aperture_type_circle: {

                            if(!should_hide(hide_element_circles)) {
                                // FAIL_IF(aperture->parameters.size() < 3, error_bad_parameter_count);
                                double radius = static_cast<float>(aperture->parameters[0]) / 2;
                                CHECK(draw_circle(drawer, net, net->end, radius));
                                // DrawAperatureHole(path, p1, p2);
                            }
                        } break;

                        case aperture_type_rectangle: {

                            if(!should_hide(hide_element_rectangles)) {
                                // FAIL_IF(aperture->parameters.size() < 4, error_bad_parameter_count);
                                double p0 = static_cast<float>(aperture->parameters[0]);
                                double p1 = static_cast<float>(aperture->parameters[1]);
                                rect aperture_rect(-(p0 / 2), -(p1 / 2), p0 / 2, p1 / 2);
                                CHECK(draw_rectangle(drawer, net, aperture_rect));
                                // path.AddRectangle(apertureRectangle);
                                // DrawAperatureHole(path, p2, p3);
                            }
                        } break;

                        case aperture_type_oval: {

                            if(!should_hide(hide_element_ovals)) {
                                // FAIL_IF(aperture->parameters.size() < 4, error_bad_parameter_count);
                                double w = static_cast<float>(aperture->parameters[0]);
                                double h = static_cast<float>(aperture->parameters[1]);
                                CHECK(draw_capsule(drawer, net, w, h));
                                // CreateOblongPath(path, p0, p1);
                                // DrawAperatureHole(path, p2, p3);
                            }
                        } break;

                        case aperture_type_polygon: {

                            if(!should_hide(hide_element_polygons)) {
                                FAIL_IF(aperture->parameters.size() < 5, error_bad_parameter_count);
                                double p0 = static_cast<float>(aperture->parameters[0]);
                                double p1 = static_cast<float>(aperture->parameters[1]);
                                double p2 = static_cast<float>(aperture->parameters[2]);
                                CHECK(fill_polygon(drawer, p0, static_cast<int>(p1), p2));
                                // DrawAperatureHole(path, p3, p4);
                            }
                        } break;

                        case aperture_type_macro: {

                            if(!should_hide(hide_element_macros)) {
                                CHECK(draw_macro(drawer, net, aperture));
                            }
                        } break;

                        default:
                            break;
                        }
                    } break;

                    // interpolate the aperture
                    case aperture_state_on:

                        switch(net->interpolation_method) {

                        // straight line
                        case interpolation_linear:
                            if(aperture->parameters.size() < 1) {
                                LOG_ERROR("Missing parameters for linear interpolation!?");
                            } else {
                                if(!should_hide(hide_element_lines)) {
                                    if(aperture->aperture_type != aperture_type_circle) {
                                        // LOG_DEBUG("{}", aperture->aperture_type);
                                    }
                                    CHECK(draw_linear_interpolation(drawer, net, aperture));
                                }
                            }
                            break;

                            // arc

                        case interpolation_clockwise_circular:
                        case interpolation_counterclockwise_circular:
                            if(aperture->parameters.size() < 1) {
                                LOG_ERROR("Missing parameters for arc!?");
                            } else {
                                if(!should_hide(hide_element_arcs)) {
                                    CHECK(draw_arc(drawer, net, aperture->parameters[0]));
                                }
                            }
                            break;

                        default:
                            break;
                        }
                        break;
                    }
                    break;
                }
            }
        }
        if(drawer.show_progress) {
            LOG_DEBUG("DRAW COMPLETE, {} nets took {} seconds", num_nets, timer.elapsed_seconds());
        }
        return ok;
    }

}    // namespace gerber_lib
