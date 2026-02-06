#pragma once

#include <iostream>
#include <iterator>
#include <string>
#include <locale>
#include <map>
#include <chrono>
#include <expected>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(__linux__)
#include <sys/ptrace.h>
#include <fstream>
#include <string>
#endif

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

    template <typename T, typename S> bool map_get_if_found(std::map<T, S> const &m, T const &key, S *const value)
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

    enum class ParseError {
        InvalidInput,
        OutOfRange
    };

    std::expected<double, ParseError> double_from_string_view(std::string_view sv);

    namespace util
    {
        //////////////////////////////////////////////////////////////////////
        // DEFER / SCOPED admin

        template <typename FUNC> struct defer_finalizer
        {
            FUNC lambda;
            bool cancelled;

            template <typename T> explicit defer_finalizer(T &&f) : lambda(std::forward<T>(f)), cancelled(false)
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

    }    // namespace util

}    // namespace gerber_util

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

//////////////////////////////////////////////////////////////////////
// DebugBreak

#if defined(_MSC_VER)
// Microsoft Visual C++
#define DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
// Clang or GCC
#if defined(__i386__) || defined(__x86_64__)
// x86/x64: Use the 'int 3' instruction
#define DEBUG_BREAK() __asm__ volatile("int $3")
#elif defined(__arm64__) || defined(__aarch64__)
// ARM64 (Apple Silicon / Raspberry Pi etc)
#define DEBUG_BREAK() __asm__ volatile("brk #0")
#elif defined(__arm__)
// ARM32
#define DEBUG_BREAK() __asm__ volatile("bkpt #0")
#else
// Fallback for other architectures (e.g., RISC-V)
#include <signal.h>
#define DEBUG_BREAK() raise(SIGTRAP)
#endif
#else
// Absolute fallback
#include <signal.h>
#define DEBUG_BREAK() raise(SIGTRAP)
#endif

//////////////////////////////////////////////////////////////////////

inline bool is_debugger_present()
{
#if defined(_WIN32)
    // Windows: The simplest case
    return IsDebuggerPresent() != 0;

#elif defined(__APPLE__)
    // macOS: Checks the kinfo_proc flags for the P_TRACED bit
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    struct kinfo_proc info;
    size_t size = sizeof(info);
    info.kp_proc.p_flag = 0;

    if(sysctl(mib, 4, &info, &size, NULL, 0) == 0) {
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    }
    return false;

#elif defined(__linux__)
    // Linux: Reads /proc/self/status to see if TracerPid is non-zero
    std::ifstream infile("/proc/self/status");
    std::string line;
    while(std::getline(infile, line)) {
        if(line.find("TracerPid:") == 0) {
            // TracerPid is the PID of the process debugging this one
            int pid = std::stoi(line.substr(10));
            return pid != 0;
        }
    }
    return false;
#else
    return false;
#endif
}

//////////////////////////////////////////////////////////////////////

#define SAFE_TRAP()                 \
    do {                            \
        if(is_debugger_present()) { \
            DEBUG_BREAK();          \
        }                           \
    } while(0)
