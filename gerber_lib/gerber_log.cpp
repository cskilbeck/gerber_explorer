//////////////////////////////////////////////////////////////////////

#include "gerber_error.h"

//////////////////////////////////////////////////////////////////////

namespace
{
    using namespace std::chrono;

    time_point<system_clock> log_startup_timestamp{ system_clock::now() };

    constexpr char const *black = "\x1b[30m";
    constexpr char const *red = "\x1b[31m";
    constexpr char const *green = "\x1b[32m";
    constexpr char const *yellow = "\x1b[33m";
    constexpr char const *blue = "\x1b[34m";
    constexpr char const *magenta = "\x1b[35m";
    constexpr char const *cyan = "\x1b[36m";
    constexpr char const *white = "\x1b[37m";
    constexpr char const *default_color = "\x1b[39m";
    constexpr char const *reset_color = "\x1b[0m";

    constexpr char const *log_level_colors[] = { green, cyan, yellow, magenta, red, red };
    constexpr char const *log_level_names[] = { "D", "V", "I", "W", "E", "F" };

    //////////////////////////////////////////////////////////////////////

    constexpr char const *log_color(gerber_lib::gerber_log_level level)
    {
        int l = static_cast<int>(level);
        return log_level_colors[l];
    }

    //////////////////////////////////////////////////////////////////////

    constexpr char const *log_name(gerber_lib::gerber_log_level level)
    {
        int l = static_cast<int>(level);
        return log_level_names[l];
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

namespace gerber_lib
{
    gerber_log_emitter_function log_emitter_function{ nullptr };

    gerber_log_level log_level{ log_level_error };

    //////////////////////////////////////////////////////////////////////

    void gerber_log(gerber_log_level level, char const *context, char const *fmt, std::format_args const &fmt_args)
    {
        time_point<system_clock> now = system_clock::now();
        auto nanos = duration_cast<nanoseconds>(now - log_startup_timestamp).count();
        auto micros = nanos / 1000;
        auto micro10 = (nanos / 100) % 10;
        char const *level_name = log_name(level);
        char const *level_color = log_color(level);
        std::string message = std::vformat(fmt, fmt_args);
        std::string log_message = std::format("{:010d}.{} {}{} {:<12.12s} {}{}", micros, micro10, level_color, level_name, context, reset_color, message);
        log_emitter_function(log_message.c_str());

        if(level == log_level_fatal) {
            exit(1);
        }
    }

}    // namespace gerber_lib
