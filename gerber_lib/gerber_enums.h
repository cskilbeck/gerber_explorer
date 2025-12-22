#pragma once

#include <format>
#include <map>

#include <stdint.h>

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    enum gerber_opcode
    {
        opcode_nop = 0,           // No operation.
        opcode_push_value,        // Push the value onto the stack.
        opcode_push_parameter,    // Push parameter onto stack.
        opcode_pop_parameter,     // Pop parameter from stack.
        opcode_add,               // Mathmatical add operation.
        opcode_subtract,          // Mathmatical subtract operation.
        opcode_multiply,          // Mathmatical multiply operation.
        opcode_divide,            // Mathmatical divide operation.
        opcode_open_bracket,      // Open bracket
        opcode_close_bracket,     // Close bracket
        opcode_unary_minus,       // -x
        opcode_unary_plus,        // +x
        opcode_primitive,         // Draw macro primitive.
        opcode_num_opcodes,
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_aperture_type
    {
        aperture_type_none,
        aperture_type_circle,
        aperture_type_rectangle,
        aperture_type_oval,
        aperture_type_polygon,
        aperture_type_macro,            // a RS274X macro.
        aperture_type_macro_circle,     // a RS274X circle macro.
        aperture_type_macro_outline,    // a RS274X outline macro.
        aperture_type_macro_polygon,    // a RS274X polygon macro.
        aperture_type_macro_moire,      // a RS274X moire macro.
        aperture_type_macro_thermal,    // a RS274X thermal macro.
        aperture_type_macro_line20,     // a RS274X line (code 20) macro.
        aperture_type_macro_line21,     // a RS274X line (code 21) macro.
        aperture_type_macro_line22      // a RS274X line (code 22) macro.
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_aperture_state
    {
        aperture_state_off,
        aperture_state_on,
        aperture_state_flash
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_unit
    {
        unit_unspecified,
        unit_inch,
        unit_millimeter
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_polarity
    {
        polarity_positive,    // draw "positive", using the current level's polarity.
        polarity_negative,    // draw "negative", reversing the current level's polarity.
        polarity_dark,        // add to the current rendering.
        polarity_clear        // subtract from the current rendering.
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_omit_zeros
    {
        omit_zeros_leading,
        omit_zeros_trailing,
        omit_zeros_explicit,
        omit_zeros_unspecified
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_coordinate
    {
        coordinate_absolute,
        coordinate_incremental
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_interpolation
    {
        interpolation_linear,
        // interpolation_drill_slot,
        interpolation_clockwise_circular,
        interpolation_counterclockwise_circular,
        interpolation_region_start,
        interpolation_region_end
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_file_type
    {
        file_type_rs274x,
        file_type_drill,
        // file_type_pick_and_place
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_knockout_type
    {
        knockout_type_no_knockout,
        knockout_type_fixed_knockout,
        knockout_type_border
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_mirror_state
    {
        mirror_state_none = 0,
        mirror_state_flip_a = 1,
        mirror_state_flip_b = 2,
        mirror_state_flip_ab = 3
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_axis_select
    {
        axis_select_none,
        axis_select_swap_ab
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_image_justify
    {
        image_justify_none,
        image_justify_lower_left,
        image_justify_centre
    };

    //////////////////////////////////////////////////////////////////////

    enum gerber_primitive_code
    {
        primitive_code_comment = 0,
        primitive_code_circle = 1,
        primitive_code_vector_line_2 = 2,
        primitive_code_outline = 4,
        primitive_code_polygon = 5,
        primitive_code_moire = 6,
        primitive_code_thermal = 7,
        primitive_code_vector_line = 20,
        primitive_code_center_line = 21,
    };

    //////////////////////////////////////////////////////////////////////

    enum circle_parameters : int
    {
        circle_exposure = 0,
        circle_diameter,
        circle_centre_x,
        circle_centre_y,
        circle_rotation,
        circle_num_parameters
    };

    //////////////////////////////////////////////////////////////////////

    enum outline_parameters : int
    {
        outline_exposure = 0,
        outline_number_of_points,
        outline_first_x,
        outline_first_y,
        outline_rotation,
        outline_num_parameters
    };

    //////////////////////////////////////////////////////////////////////

    enum polygon_parameters : int
    {
        polygon_exposure = 0,
        polygon_number_of_sides,
        polygon_centre_x,
        polygon_centre_y,
        polygon_diameter,
        polygon_rotation,
        polygon_num_parameters
    };

    //////////////////////////////////////////////////////////////////////

    enum moire_parameters : int
    {
        moire_centre_x = 0,
        moire_centre_y,
        moire_outside_diameter,
        moire_circle_line_width,
        moire_gap_width,
        moire_number_of_circles,
        moire_crosshair_line_width,
        moire_crosshair_length,
        moire_rotation,
        moire_num_parameters
    };

    //////////////////////////////////////////////////////////////////////

    enum thermal_parameters : int
    {
        thermal_centre_x = 0,
        thermal_centre_y,
        thermal_outside_diameter,
        thermal_inside_diameter,
        thermal_crosshair_line_width,
        thermal_rotation,
        thermal_num_parameters
    };

    //////////////////////////////////////////////////////////////////////

    enum line_20_parameters : int
    {
        line_20_exposure = 0,
        line_20_line_width,
        line_20_start_x,
        line_20_start_y,
        line_20_end_x,
        line_20_end_y,
        line_20_rotation,
        line_20_num_parameters
    };

    //////////////////////////////////////////////////////////////////////

    enum line_21_parameters : int
    {
        line_21_exposure = 0,
        line_21_line_width,
        line_21_line_height,
        line_21_centre_x,
        line_21_centre_y,
        line_21_rotation,
        line_21_num_parameters
    };

    //////////////////////////////////////////////////////////////////////

    enum line_22_parameters : int
    {
        line_22_exposure = 0,
        line_22_line_width,
        line_22_line_height,
        line_22_lower_left_x,
        line_22_lower_left_y,
        line_22_rotation,
        line_22_num_parameters
    };

}    // namespace gerber_lib

// this relies on an extern std::map<ENUM_TYPE, char const *> ENUM_TYPE_names_map; in gerber_lib namespace

#define GERBER_MAKE_ENUM_FORMATTER(GERBER_ENUM)                                                                             \
    namespace gerber_lib::gerber_enum_names                                                                                 \
    {                                                                                                                       \
        extern std::map<GERBER_ENUM, char const *> GERBER_ENUM##_names_map;                                                 \
    }                                                                                                                       \
    template <> struct std::formatter<::gerber_lib::GERBER_ENUM> : std::formatter<std::string>                              \
    {                                                                                                                       \
        auto format(::gerber_lib::GERBER_ENUM const &e, std::format_context &ctx) const                                     \
        {                                                                                                                   \
            auto f = ::gerber_lib::gerber_enum_names::GERBER_ENUM##_names_map.find(e);                                      \
            if(f != ::gerber_lib::gerber_enum_names::GERBER_ENUM##_names_map.end()) {                                       \
                return std::format_to(ctx.out(), "{}", f->second);                                                          \
            }                                                                                                               \
            return std::format_to(ctx.out(), "@ERROR: Can't find enum value {} for {}", static_cast<int>(e), #GERBER_ENUM); \
        }                                                                                                                   \
    }

GERBER_MAKE_ENUM_FORMATTER(gerber_opcode);
GERBER_MAKE_ENUM_FORMATTER(gerber_aperture_type);
GERBER_MAKE_ENUM_FORMATTER(gerber_aperture_state);
GERBER_MAKE_ENUM_FORMATTER(gerber_unit);
GERBER_MAKE_ENUM_FORMATTER(gerber_polarity);
GERBER_MAKE_ENUM_FORMATTER(gerber_interpolation);
GERBER_MAKE_ENUM_FORMATTER(gerber_image_justify);
GERBER_MAKE_ENUM_FORMATTER(gerber_mirror_state);
GERBER_MAKE_ENUM_FORMATTER(gerber_axis_select);
GERBER_MAKE_ENUM_FORMATTER(gerber_knockout_type);
GERBER_MAKE_ENUM_FORMATTER(gerber_primitive_code);
