#include <map>

#include "gerber_enums.h"

#define ENUM_NAMES_MAP(GERBER_ENUM) std::map<GERBER_ENUM, char const *> GERBER_ENUM##_names_map

namespace gerber_lib::gerber_enum_names
{
    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_aperture_type) = {
        { aperture_type_none, "none" },
        { aperture_type_circle, "circle" },
        { aperture_type_rectangle, "rectangle" },
        { aperture_type_oval, "oval" },
        { aperture_type_polygon, "polygon" },
        { aperture_type_macro, "macro" },
        { aperture_type_macro_circle, "macro_circle" },
        { aperture_type_macro_outline, "macro_outline" },
        { aperture_type_macro_polygon, "macro_polygon" },
        { aperture_type_macro_moire, "macro_moire" },
        { aperture_type_macro_thermal, "macro_thermal" },
        { aperture_type_macro_line20, "macro_line20" },
        { aperture_type_macro_line21, "macro_line21" },
        { aperture_type_macro_line22, "macro_line22" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_interpolation) = {
        { interpolation_linear, "linear" },
        // { interpolation_drill_slot, "drill_slot" },
        { interpolation_clockwise_circular, "clockwise_circular" },
        { interpolation_counterclockwise_circular, "counterclockwise_circular" },
        { interpolation_region_start, "region_start" },
        { interpolation_region_end, "region_end" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_aperture_state) = {
        { aperture_state_off, "off" },
        { aperture_state_on, "on" },
        { aperture_state_flash, "flash" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_opcode) = {
        { opcode_nop, "nop " },
        { opcode_push_value, "push" },
        { opcode_push_parameter, "push_parameter" },
        { opcode_pop_parameter, "pop_parameter" },
        { opcode_add, "add" },
        { opcode_subtract, "subtract" },
        { opcode_multiply, "multiply" },
        { opcode_divide, "divide" },
        { opcode_primitive, "primitive" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_unit) = {
        { unit_unspecified, "unspecified" },
        { unit_inch, "inch" },
        { unit_millimeter, "millimeter" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_polarity) = {
        { polarity_positive, "positive" },
        { polarity_negative, "negative" },
        { polarity_dark, "dark" },
        { polarity_clear, "clear" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_image_justify) = {
        { image_justify_none, "justify_none" },
        { image_justify_lower_left, "justify_lower_left" },
        { image_justify_centre, "justify_centre" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_mirror_state) = {
        { mirror_state_none, "state_none" },
        { mirror_state_flip_a, "state_flip_a" },
        { mirror_state_flip_b, "state_flip_b" },
        { mirror_state_flip_ab, "state_flip_ab" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_axis_select) = {
        { axis_select_none, "select_none" },
        { axis_select_swap_ab, "select_swap_ab" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_knockout_type) = {
        { knockout_type_no_knockout, "type_no_knockout" },
        { knockout_type_fixed_knockout, "type_fixed_knockout" },
        { knockout_type_border, "type_border" },
    };

    //////////////////////////////////////////////////////////////////////

    ENUM_NAMES_MAP(gerber_primitive_code) = {
        { primitive_code_comment, "comment" },    //
        { primitive_code_circle, "circle" },
        { primitive_code_vector_line_2, "vector_line_2" },
        { primitive_code_outline, "outline" },
        { primitive_code_polygon, "polygon" },
        { primitive_code_moire, "moire" },
        { primitive_code_thermal, "thermal" },
        { primitive_code_vector_line, "vector_line" },
        { primitive_code_center_line, "center_line" },
    };

}    // namespace gerber_lib