#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct layer_t
{
    std::string filename;
    std::string color;
    bool visible{ false };
    bool inverted{ false };
    int draw_mode{ 0 };
    int index{ 0 };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(layer_t, filename, color, visible, inverted, draw_mode, index)
};

#define SETTINGS_FIELDS              \
    X(bool, wireframe, false)        \
    X(bool, show_axes, true)         \
    X(bool, show_extent, true)       \
    X(bool, flip_x, false)           \
    X(bool, flip_y, false)           \
    X(bool, window_maximized, false) \
    X(float, outline_width, 1.0f)    \
    X(int, window_width, 800)        \
    X(int, window_height, 600)       \
    X(int, window_xpos, 100)         \
    X(int, window_ypos, 100)         \
    X(std::vector<layer_t>, files, {})

struct settings_t
{
#define X(type, name, ...) type name = __VA_ARGS__;
    SETTINGS_FIELDS
#undef X

    void save(std::filesystem::path const &path);
    void load(std::filesystem::path const &path);

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
