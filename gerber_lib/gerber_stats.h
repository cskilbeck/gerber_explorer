//////////////////////////////////////////////////////////////////////

#pragma once

#include <list>
#include <vector>
#include <string>
#include <format>

#include "gerber_enums.h"
#include "gerber_error.h"
#include "gerber_reader.h"

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    struct gerber_aperture_info;
    struct gerber_reader;

    //////////////////////////////////////////////////////////////////////

    struct gerber_stats
    {
        std::list<gerber_error> errors;
        std::vector<gerber_aperture_info *> apertures;
        std::vector<gerber_aperture_info *> d_codes;

        int level_count{};
        int g0{};
        int g1{};
        int g2{};
        int g3{};
        int g4{};
        int g36{};
        int g37{};
        int g54{};
        int g55{};
        int g70{};
        int g71{};
        int g74{};
        int g75{};
        int g90{};
        int g91{};
        int d1{};
        int d2{};
        int d3{};
        int m0{};
        int m1{};
        int m2{};

        int unknown_g_codes{};
        int unknown_d_codes{};
        int unknown_m_codes{};

        int d_code_errors{};

        int x_count{};
        int y_count{};
        int i_count{};
        int j_count{};
        int star_count{};
        int unknown_count{};

        gerber_stats() = default;

        ~gerber_stats()
        {
            cleanup();
        }

        void cleanup();

        //////////////////////////////////////////////////////////////////////

        void add_aperture(int level, int number, gerber_aperture_type type, double parameter[5]);
        void add_to_d_list(int number);
        void add_new_d_list(int number);
        gerber_error_code increment_d_list_count(int number, int count, int line);

        //////////////////////////////////////////////////////////////////////

        template <typename... args>
        gerber_error_code error(gerber_reader const &reader, gerber_error_code code, char const *fmt = nullptr, args &&...arguments)
        {
            LOG_CONTEXT("error", debug);

            std::string error_msg{ "!" };

            if(fmt != nullptr) {
                error_msg = std::vformat(fmt, std::make_format_args(arguments...));
            }

            std::string error_text = get_error_text(code);

            std::string error_message = std::format("error {} ({}) at line {}: {}", static_cast<int>(code), error_text, reader.line_number, error_msg);

            errors.emplace_back(gerber_error(code, error_message, reader.filename, reader.line_number));
            LOG_ERROR("{}", error_message);
            return code;
        }
    };

}    // namespace gerber_lib
