#pragma once

#include <iostream>
#include <iterator>
#include <string>
#include <locale>
#include <codecvt>
#include <map>
#include <chrono>

namespace gerber_util
{
    //////////////////////////////////////////////////////////////////////

    struct gerber_timer
    {
        gerber_timer() = default;

        std::chrono::time_point<std::chrono::high_resolution_clock> time_point_begin;

        void reset()
        {
            time_point_begin = std::chrono::high_resolution_clock::now();
        }

        double elapsed_seconds() const
        {
            auto time_point_end = std::chrono::high_resolution_clock::now();
            auto diff = time_point_end - time_point_begin;
            auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(diff);
            return microseconds.count() / 1000000.0;
        }
    };

    //////////////////////////////////////////////////////////////////////

    std::wstring utf16_from_utf8(std::string const &s);

    std::string to_lowercase(std::string const &s);

    //////////////////////////////////////////////////////////////////////

    template <typename... args> void print(char const *fmt, args &&...arguments)
    {
        std::vformat_to(std::ostream_iterator<char>(std::cout), fmt, std::make_format_args(arguments...));
    }

    //////////////////////////////////////////////////////////////////////
    // get something from a map

    template <typename T, typename S> bool map_get_if_found(std::map<T, S> const &m, T const &key, S * const value)
    {
        auto f = m.find(key);
        if(f != m.end()) {
            *value = f->second;
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // get number of elements in an array

    template <typename T, size_t N> constexpr size_t array_length(T const (&)[N])
    {
        return N;
    }

    namespace util
    {
        //////////////////////////////////////////////////////////////////////
        // DEFER / SCOPED admin

        template <typename FUNC> struct defer_finalizer
        {
            FUNC lambda;
            bool cancelled;

            template <typename T> defer_finalizer(T &&f) : lambda(std::forward<T>(f)), cancelled(false)
            {
            }

            defer_finalizer() = delete;
            defer_finalizer(defer_finalizer const &) = delete;
            defer_finalizer(defer_finalizer &&) = delete;

            void cancel()
            {
                cancelled = true;
            }

            ~defer_finalizer()
            {
                if(!cancelled) {
                    lambda();
                }
            }
        };

        [[maybe_unused]] static struct
        {
            template <typename F> [[nodiscard]] defer_finalizer<F> operator<<(F &&f)
            {
                return defer_finalizer<F>(std::forward<F>(f));
            }
        } deferrer;

    }    // namespace gerber_util

}    // namespace gerber_lib

//////////////////////////////////////////////////////////////////////
// SCOPED: assign SCOPED(<lambda>) to a variable which calls the lambda when it goes out of scope
// it can be cancelled...
//
// e.g.
//
// {
//     foo *f = new foo();
//     auto cleanup = SCOPED([&]() { delete f; });
//     ...
//     if(something) {
//          delete f;
//          cleanup.cancel();
//     }
//     ...
// } <- cleanup lambda called here (if cleanup.cancel() was not called)
//

#define SCOPED gerber_lib::util::deferrer <<

//////////////////////////////////////////////////////////////////////
// DEFER: for convenience, DEFER auto generates a variable in current scope (using capture by VALUE!)
// can't be cancelled...
//
// e.g.
//
// {
//     foo *f = new foo();
//     DEFER(delete f);
//     ...
//     ...
// } <- delete f is called here

#define _DEFER_TOKENPASTE(x, y) x##y
#define _DEFER_TOKENPASTE2(x, y) _DEFER_TOKENPASTE(x, y)

#define DEFER(X) auto _DEFER_TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = gerber_lib::util::deferrer << [=] { X; }

//////////////////////////////////////////////////////////////////////
// if there's a `to_string()` member function, you can use this
// to make a type formattable. If there's no to_string() method, compile fails

#define GERBER_MAKE_FORMATTER(GERBER_TYPE)                                       \
    template <> struct std::formatter<GERBER_TYPE> : std::formatter<std::string> \
    {                                                                            \
        auto format(GERBER_TYPE const &e, std::format_context &ctx) const        \
        {                                                                        \
            return std::format_to(ctx.out(), "{}", e.to_string());               \
        }                                                                        \
    }
