//////////////////////////////////////////////////////////////////////

#include <filesystem>
#include <cstring>

#include "gerber_error.h"
#include "gerber_reader.h"

LOG_CONTEXT("line_reader", info);

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_reader::open(char const *data, size_t size)
    {
        if(data == nullptr) {
            return error_invalid_parameter;
        }
        if(size == 0) {
            return error_empty_file;
        }
        file_data = data;
        file_size = size;
        file_pos = 0;
        line_number = 1;
        filename = std::format("mem:0x{}:{}", file_data, file_size);
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_reader::open(char const *file_path)
    {
        if(file_path == nullptr) {
            return error_internal_bad_pointer;
        }

        if(!std::filesystem::exists(file_path)) {
            return error_file_not_found;
        }

        if(!std::filesystem::is_regular_file(file_path)) {
            return error_invalid_file_attributes;
        }

        size_t file_bytes = std::filesystem::file_size(file_path);

        if(file_bytes == 0) {
            return error_empty_file;
        }

        std::ifstream in_stream(file_path, std::ios::binary);

        if(!in_stream.is_open()) {
            char error_msg[256];
            strerror_s(error_msg, errno);
            LOG_ERROR("Error opening file {}: {}", file_path, error_msg);
            return error_cant_open_file;
        }

        file_buffer.clear();
        file_buffer.reserve(file_bytes);
        file_buffer.assign(std::istreambuf_iterator(in_stream), std::istreambuf_iterator<char>());

        file_data = file_buffer.data();
        file_size = file_buffer.size();

        filename.assign(file_path);
        LOG_VERBOSE("Opened file {}, {} bytes available", filename, file_size);
        file_pos = 0;
        line_number = 1;
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_reader::close()
    {
        file_data = nullptr;
        file_size = 0;
        file_buffer.clear();
    }

    //////////////////////////////////////////////////////////////////////

    bool gerber_reader::eof() const
    {
        return file_pos >= file_size;
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_reader::skip(size_t num_chars)
    {
        for(; num_chars != 0 && !eof(); --num_chars) {
            skip_whitespace();
            file_pos += 1;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_reader::rewind(size_t num_chars)
    {
        for(; num_chars != 0 && file_pos != 0; --num_chars) {
            file_pos -= 1;
            skip_whitespace_reverse();
        }
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_reader::peek(char *c)
    {
        if(c == nullptr) {
            return error_internal_bad_pointer;
        }
        skip_whitespace();
        if(eof()) {
            return error_end_of_file;
        }
        *c = file_data[file_pos];
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_reader::read_char(char *c)
    {
        if(c == nullptr) {
            return error_internal_bad_pointer;
        }
        skip_whitespace();
        if(eof()) {
            return error_end_of_file;
        }
        *c = file_data[file_pos];
        file_pos += 1;
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_reader::read_short(uint32_t *c, int count)
    {
        if(c == nullptr) {
            return error_internal_bad_pointer;
        }
        if(count > 4) {
            return error_internal_bad_argument;
        }
        uint32_t r = 0;
        for(int i = 0; i < count; ++i) {
            char a;
            CHECK(read_char(&a));
            r <<= 8;
            r |= a;
        }
        *c = r;
        return ok;
    }

    //////////////////////////////////////////////////////////////////////
    // skip_whitespace is not locale-aware because neither is the spec

    void gerber_reader::skip_whitespace()
    {
        while(!eof()) {
            char c = file_data[file_pos];
            switch(c) {
            case '\n':
                line_number += 1;
            case ' ':
            case '\r':
            case '\f':
            case '\t':
            case '\v':
                file_pos += 1;
                continue;
            }
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // skip_whitespace_reverse is not locale-aware because neither is the spec

    void gerber_reader::skip_whitespace_reverse()
    {
        while(file_pos != 0) {
            char c = file_data[file_pos];
            switch(c) {
            case '\n':
                line_number -= 1;
            case ' ':
            case '\r':
            case '\f':
            case '\t':
            case '\v':
                file_pos -= 1;
                continue;
            }
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////
    // read_until does NOT skip whitespace...

    gerber_error_code gerber_reader::read_until(std::string *s, char value)
    {
        std::string result;
        while(!eof()) {
            char c = file_data[file_pos];
            if(c == value) {
                if(s != nullptr) {
                    *s = result;
                }
                return ok;
            }
            file_pos += 1;
            result.push_back(c);
        }
        return error_missing_terminator;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_reader::get_int(int *value, size_t *length)
    {
        if(value == nullptr) {
            return error_internal_bad_pointer;
        }

        size_t len = 0;
        int number = 0;

        char c;
        CHECK(peek(&c));
        bool negate = c == '-';

        if(negate || c == '+') {
            skip(1);
        }

        while(!eof()) {
            CHECK(read_char(&c));
            if(!isdigit(c)) {
                rewind(1);
                break;
            }
            number *= 10;
            number += c - '0';
            len += 1;
        }
        if(len == 0) {
            LOG_ERROR("Missing int at line {}", line_number);
            return error_missing_integer_value;
        }
        if(negate) {
            number = -number;
        }
        *value = number;
        if(length != nullptr) {
            *length = len;
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_reader::get_double(double *value, size_t *length)
    {
        if(value == nullptr) {
            return error_internal_bad_pointer;
        }

        // Doesn't handle exponent because it's not supported in the spec

        size_t len = 0;
        double number = 0.0;

        double divide = 1.0;
        bool found_decimal_point{ false };
        size_t num_digits = 0;

        char c;
        CHECK(peek(&c));
        bool negate = c == '-';

        if(negate || c == '+') {
            skip(1);
        }

        while(!eof()) {
            CHECK(read_char(&c));
            if(c == '.') {
                if(found_decimal_point) {
                    break;
                }
                found_decimal_point = true;
                len += 1;
                continue;
            }
            if(!isdigit(c)) {
                rewind(1);
                break;
            }
            num_digits += 1;
            double digit = c - '0';
            if(found_decimal_point) {
                divide /= 10.0;
                number += digit * divide;
            } else {
                number *= 10.0;
                number += digit;
            }
            len += 1;
        }
        if(num_digits == 0) {
            LOG_ERROR("Missing real number at line {}", line_number);
            return error_missing_real_number_value;
        }
        if(length != nullptr) {
            *length = len;
        }
        *value = number;
        return ok;
    }

}    // namespace gerber_lib
