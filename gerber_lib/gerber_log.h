//////////////////////////////////////////////////////////////////////

#pragma once

#include <chrono>
#include <string>
#include <format>

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    enum gerber_log_level
    {
        log_level_debug = 0,
        log_level_verbose = 1,
        log_level_info = 2,
        log_level_warning = 3,
        log_level_error = 4,
        log_level_fatal = 5,
        log_level_none = 6
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_log_context
    {
        char const *context;
        gerber_log_level max_level;
    };

    //////////////////////////////////////////////////////////////////////

    typedef int (*gerber_log_emitter_function)(char const *);

    extern gerber_log_level log_level;

    extern gerber_log_emitter_function log_emitter_function;

    //////////////////////////////////////////////////////////////////////

    inline void log_set_level(gerber_log_level level)
    {
        log_level = level;
    }

    //////////////////////////////////////////////////////////////////////

    inline void log_set_emitter_function(gerber_log_emitter_function function)
    {
        log_emitter_function = function;
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_log(gerber_log_level level, char const *context, char const *fmt, std::format_args const &fmt_args);

    //////////////////////////////////////////////////////////////////////

    template <typename... args> constexpr void log(gerber_log_level level, gerber_log_context const &context, char const *fmt, args &&...arguments)
    {
        if(level == log_level_fatal || (level >= log_level && level >= context.max_level)) {
            gerber_log(level, context.context, fmt, std::make_format_args(arguments...));
        }
        if(level == log_level_fatal) {
            __debugbreak();
        }
    }

}    // namespace gerber_lib

//////////////////////////////////////////////////////////////////////

#define LOG_CONTEXT(context, max_level)                              \
    static constexpr ::gerber_lib::gerber_log_context __log_context  \
    {                                                                \
        context, gerber_lib::gerber_log_level::log_level_##max_level \
    }

#define LOG_DEBUG(msg, ...) ::gerber_lib::log(::gerber_lib::log_level_debug, __log_context, msg, ##__VA_ARGS__)
#define LOG_VERBOSE(msg, ...) ::gerber_lib::log(::gerber_lib::log_level_verbose, __log_context, msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...) ::gerber_lib::log(::gerber_lib::log_level_info, __log_context, msg, ##__VA_ARGS__)
#define LOG_WARNING(msg, ...) ::gerber_lib::log(::gerber_lib::log_level_warning, __log_context, msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) ::gerber_lib::log(::gerber_lib::log_level_error, __log_context, msg, ##__VA_ARGS__)
#define LOG_FATAL(msg, ...) ::gerber_lib::log(::gerber_lib::log_level_fatal, __log_context, msg, ##__VA_ARGS__)

#define GERBER_ASSERT(x)                                                             \
    do                                                                               \
        if(!(x))                                                                     \
            LOG_FATAL("ASSERT FAILED: {} at line {} of {}", #x, __LINE__, __FILE__); \
    while(false)
