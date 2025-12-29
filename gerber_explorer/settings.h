#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#define SETTINGS_FIELDS                                                                                                                                        \
    X(bool, wireframe)                                                                                                                                         \
    X(std::vector<std::string>, files)

struct settings
{
#define X(type, name) type name;
    SETTINGS_FIELDS
#undef X
};

inline void to_json(nlohmann::json& j, const settings& s)
{
#define X(type, name) j[#name] = s.name;
    SETTINGS_FIELDS
#undef X
}

inline void from_json(const nlohmann::json& j, settings& s)
{
#define X(type, name) s.name = j.value(#name, type{});
    SETTINGS_FIELDS
#undef X
}
