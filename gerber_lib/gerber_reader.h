#pragma once

#include <format>
#include <string>
#include <fstream>

#include "gerber_error.h"

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    struct gerber_reader
    {
        gerber_reader() = default;

        gerber_error_code open(char const *file_path);

        gerber_error_code open(char const *data, size_t size);

        void close();

        bool eof() const;

        gerber_error_code peek(char *c);
        gerber_error_code read_char(char *c);
        gerber_error_code read_short(uint32_t *c, int count);
        gerber_error_code read_until(std::string *s, char value);
        gerber_error_code get_int(int *value, size_t *length = nullptr);
        gerber_error_code get_double(double *value, size_t *length = nullptr);

        void skip(size_t num_chars);
        void rewind(size_t num_chars);
        void skip_whitespace();
        void skip_whitespace_reverse();

        //////////////////////////////////////////////////////////////////////

        int line_number{};

        char const *file_data;
        size_t file_size;
        size_t file_pos{};

        std::ifstream input_stream;
        std::string filename;
        std::vector<char> file_buffer;
    };

    //////////////////////////////////////////////////////////////////////

    enum tokenize_option
    {
        tokenize_remove_empty,
        tokenize_keep_empty,
    };

    template <typename T> void tokenize(std::string_view const str, T &tokens, std::string_view const delimiter, tokenize_option option)
    {
        size_t start = str.find_first_not_of(delimiter);
        while(start != std::string::npos) {
            size_t end = str.find_first_of(delimiter, start);
            if(end != start || option == tokenize_keep_empty) {
                tokens.push_back(typename T::value_type(str.substr(start, end - start)));
            }
            start = str.find_first_not_of(delimiter, end);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // YOINK: this is what giving zero fucks about performance looks like

    template <typename T> std::string join(T const &values, std::string_view const join_with)
    {
        std::string result;
        std::string joiner;
        for(auto const &s : values) {
            result = std::format("{}{}{}", result, joiner, s);
            joiner = join_with;
        }
        return result;
    }
}    // namespace gerber_lib
