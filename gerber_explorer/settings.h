#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#define SETTINGS_FIELDS                                                                                                                                        \
    X(bool, wireframe, false)                                                                                                                                  \
    X(std::vector<std::string>, files, {})

struct settings_t
{
#define X(type, name, val) type name = val;
    SETTINGS_FIELDS
#undef X

    void save();
    void load();
};

inline void to_json(nlohmann::json &j, const settings_t &s)
{
#define X(type, name, val) j[#name] = s.name;
    SETTINGS_FIELDS
#undef X
}

inline void from_json(const nlohmann::json &j, settings_t &s)
{
#define X(type, name, val) s.name = j.value(#name, type{});
    SETTINGS_FIELDS
#undef X
}
