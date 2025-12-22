//////////////////////////////////////////////////////////////////////

#pragma once

#include "gerber_enums.h"

namespace gerber_lib
{
    struct gerber_format
    {
        gerber_omit_zeros omit_zeros{ omit_zeros_leading };
        gerber_coordinate coordinate{ coordinate_absolute };
        int integral_part_x{};
        int decimal_part_x{};
        int integral_part_y{};
        int decimal_part_y{};
        int sequence_number_limit{};
        int general_function_limit{};
        int plot_function_limit{};
        int misc_function_limit{};

        gerber_format() = default;
    };

}    // namespace gerber_lib