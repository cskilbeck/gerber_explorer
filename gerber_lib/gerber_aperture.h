//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

#include "gerber_error.h"
#include "gerber_util.h"

namespace gerber_lib
{
    struct gerber_reader;
    struct gerber_macro_parameters;

    static constexpr int max_num_aperture_parameters = 102;

    //////////////////////////////////////////////////////////////////////

    struct gerber_aperture_info
    {
        double parameters[5]{};
        int number{};
        int level{};
        int count{};
        gerber_aperture_type aperture_type{ aperture_type_none };

        std::string to_string() const
        {
            return std::format("APERTURE_INFO: NUMBER: {}, LEVEL: {}, COUNT: {}, APERTURE_TYPE: {}", number, level, count, aperture_type);
        }

        gerber_aperture_info() = default;
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_instruction
    {
        gerber_opcode opcode{ opcode_nop };
        double double_value{};
        int int_value{};

        std::string to_string() const
        {
            return std::format("INSTRUCTION: OPCODE: {}, DOUBLE: {}, INT: {}", opcode, double_value, int_value);
        }

        gerber_instruction() = default;

        gerber_instruction(gerber_opcode code) : gerber_instruction()
        {
            opcode = code;
        }

        gerber_instruction(gerber_opcode code, double value) : gerber_instruction()
        {
            opcode = code;
            double_value = value;
        }

        gerber_instruction(gerber_opcode code, int value) : gerber_instruction()
        {
            opcode = code;
            int_value = value;
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_aperture_macro
    {
        std::vector<gerber_instruction> instructions;
        std::string name;

        std::string to_string() const
        {
            return std::format("APERTURE_MACRO: NAME: {}, INSTRUCTIONS: {}", name, instructions.size());
        }

        gerber_aperture_macro() = default;

        gerber_error_code parse_aperture_macro(gerber_reader &reader);
    };


    //////////////////////////////////////////////////////////////////////

    struct gerber_aperture
    {
        std::vector<double> parameters;
        gerber_aperture_type aperture_type{ aperture_type_none };
        gerber_aperture_macro *aperture_macro{ nullptr };
        gerber_unit unit{ unit_unspecified };
        int aperture_number{};
        std::vector<gerber_macro_parameters *> macro_parameters_list;

        std::string to_string() const
        {
            return std::format("APERTURE D{}: TYPE: {}, UNIT: {}, PARAMETERS: {}, MACRO_PARAMETERS: {}",
                               aperture_number,        //
                               aperture_type,        //
                               unit,                 //
                               parameters.size(),    //
                               macro_parameters_list.size());
        }

        std::string get_description(double scale, std::string const &units) const;

        gerber_aperture() = default;

        ~gerber_aperture();

        gerber_error_code execute_aperture_macro(double scale);
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_macro_parameters
    {
        gerber_aperture_type aperture_type{ aperture_type_none };

        std::vector<double> parameters;

        std::string to_string() const
        {
            return std::format("MACRO_PARAMETERS: APERTURE_TYPE: {}, PARAMETERS: {}", aperture_type, parameters.size());
        }

        gerber_macro_parameters() = default;
    };

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_aperture_info);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_instruction);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_aperture_macro);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_aperture);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_macro_parameters);
