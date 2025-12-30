#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#define SETTINGS_FIELDS                                                                                                                                        \
    X(bool, wireframe, false)                                                                                                                                  \
    X(bool, show_axes, true)                                                                                                                                   \
    X(bool, show_extent, true)                                                                                                                                 \
    X(bool, window_maximized, false)                                                                                                                           \
    X(int, window_width, 800)                                                                                                                                  \
    X(int, window_height, 600)                                                                                                                                 \
    X(int, window_xpos, 100)                                                                                                                                   \
    X(int, window_ypos, 100)                                                                                                                                   \
    X(std::vector<std::string>, files, {})

struct settings_t
{
#define X(type, name, val) type name = val;
    SETTINGS_FIELDS
#undef X

    void save();
    void load();

    void to_json(nlohmann::json &j)
    {
#define X(type, name, val) j[#name] = name;
        SETTINGS_FIELDS
#undef X
    }

    void from_json(const nlohmann::json &j)
    {
#define X(type, name, val) name = j.value(#name, type{});
        SETTINGS_FIELDS
#undef X
    }
};
