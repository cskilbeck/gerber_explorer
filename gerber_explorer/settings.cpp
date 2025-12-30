//////////////////////////////////////////////////////////////////////

#include <fstream>
#include <nlohmann/json.hpp>

#include "gerber_log.h"
#include "settings.h"
#include "util.h"

LOG_CONTEXT("settings", debug);

//////////////////////////////////////////////////////////////////////

namespace
{
    std::string const app_name = "gerber_explorer";
    std::string const settings_filename = "settings.json";
}

//////////////////////////////////////////////////////////////////////

void settings_t::save()
{
    nlohmann::json json;
    to_json(json, *this);
    std::string json_str = json.dump(4);
    LOG_DEBUG("{}", json_str);
    std::filesystem::path path = config_path(app_name, settings_filename);
    std::ofstream save(path);
    save << json_str;
    save.close();
}

//////////////////////////////////////////////////////////////////////

void settings_t::load()
{
    std::filesystem::path path = config_path(app_name, settings_filename);
    std::ifstream load(path);
    nlohmann::json json = nlohmann::json::parse(load);
    load.close();
    from_json(json, *this);
}
