#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace settings
{
    enum units_t
    {
        units_mm = 0,
        units_inch = 1
    };

    struct color_t
    {
        float r{ 0 };
        float g{ 0 };
        float b{ 0 };
        float a{ 1 };
        explicit operator float *()
        {
            return &r;
        }
        explicit operator float const *() const
        {
            return &r;
        }
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(color_t, r, g, b, a)
    };

    struct layer_t
    {
        std::string filename;
        std::string color;
        bool visible{ false };
        bool inverted{ false };
        int index{ 0 };

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(layer_t, filename, color, visible, inverted, index)
    };
}    // namespace settings

#define SETTINGS_FIELDS                        \
    X(bool, wireframe, false)                  \
    X(bool, show_axes, true)                   \
    X(bool, show_extent, true)                 \
    X(bool, flip_x, false)                     \
    X(bool, flip_y, false)                     \
    X(bool, window_maximized, false)           \
    X(float, outline_width, 1.0f)              \
    X(settings::color_t, outline_color, {})    \
    X(settings::color_t, background_color, {}) \
    X(int, window_width, 800)                  \
    X(int, window_height, 600)                 \
    X(int, window_xpos, 100)                   \
    X(int, window_ypos, 100)                   \
    X(int, multisamples, 1)                    \
    X(int, tesselation_quality, 1)             \
    X(bool, dynamic_tesselation, false)        \
    X(bool, view_toolbar, true)                \
    X(int, board_view, 0)                      \
    X(int, units, settings::units_mm)          \
    X(std::vector<settings::layer_t>, files, {})

struct settings_t
{
#define X(type, name, ...) type name = __VA_ARGS__;
    SETTINGS_FIELDS
#undef X

    void save(std::filesystem::path const &path);
    bool load(std::filesystem::path const &path);

    void to_json(nlohmann::json &j)
    {
#define X(type, name, ...) j[#name] = name;
        SETTINGS_FIELDS
#undef X
    }

    void from_json(const nlohmann::json &j)
    {
        LOG_CONTEXT("from_json", debug);
#define X(type, name, ...)              \
    if(j.contains(#name)) {             \
        name = j.at(#name).get<type>(); \
    } else {                            \
        name = __VA_ARGS__;             \
    }
        SETTINGS_FIELDS
#undef X
    }
};
