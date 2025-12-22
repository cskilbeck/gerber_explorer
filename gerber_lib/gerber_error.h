//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <format>

#include "gerber_log.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    constexpr char const *get_file_name(char const *const path)
    {
        char const *start_position = path;
        for(char const *c = path; *c != '\0'; ++c) {
            if(*c == '\\' || *c == '/') {
                start_position = c;
            }
        }
        if(start_position != path) {
            ++start_position;
        }
        return start_position;
    }
}    // namespace

//////////////////////////////////////////////////////////////////////

#define CHECK(x)                                                                                                                            \
    do {                                                                                                                                    \
        ::gerber_lib::gerber_error_code __error = (x);                                                                                      \
        if(__error != ::gerber_lib::ok) {                                                                                                   \
            LOG_ERROR("{}(error {}): `{}` (line {} of {})", ::gerber_lib::get_error_text(__error), static_cast<int>(__error), #x, __LINE__, \
                      get_file_name(__FILE__));                                                                                             \
            return __error;                                                                                                                 \
        }                                                                                                                                   \
    } while(false)

//////////////////////////////////////////////////////////////////////

#define FAIL_IF(condition, error_code)                                                                                                                    \
    do {                                                                                                                                                  \
        if(condition) {                                                                                                                                   \
            LOG_ERROR("{}(error {}) because `{}` (at line {} of {})", ::gerber_lib::get_error_text(error_code), static_cast<int>(error_code), #condition, \
                      __LINE__, get_file_name(__FILE__));                                                                                                 \
            return error_code;                                                                                                                            \
        }                                                                                                                                                 \
    } while(false)

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

#undef GERBER_ERROR_CODE
#undef GERBER_ERROR_CODES
#define GERBER_ERROR_CODE(a) error_##a,
#include "gerber_error_codes.h"

    enum gerber_error_code : uint32_t
    {
        ok,
        GERBER_ERROR_CODES
    };

    char const *get_error_text(gerber_error_code error_code);

    //////////////////////////////////////////////////////////////////////
    // convert uint32 to ascii, skipping zeros in the upper bytes

    inline std::string string_from_uint32(uint32_t n)
    {
        std::string s;
        while(n != 0) {
            uint32_t c = n >> 24;
            if(c != 0) {
                if(c < ' ' || c >= 127) {
                    c = '?';
                }
                s.append({ static_cast<char>(c) });
            }
            n <<= 8;
        }
        if(s.empty()) {
            s = "?";
        }
        return s;
    }

    //////////////////////////////////////////////////////////////////////
    // convert a char to a string

    inline std::string string_from_char(int c)
    {
        if(c >= ' ' && c <= 127) {
            return std::string({ static_cast<char>(c) });
        }
        return std::format("0x{:02x}", static_cast<uint8_t>(c));
    }

    //////////////////////////////////////////////////////////////////////

    struct gerber_error
    {
        gerber_error_code error_code{};
        std::string message{};
        std::string filename{};
        int line_number{};

        gerber_error() = default;

        gerber_error(gerber_error_code code, std::string const &msg, std::string const &file, int line)
            : error_code(code), message(msg), filename(file), line_number(line)
        {
        }
    };

}    // namespace gerber_lib
