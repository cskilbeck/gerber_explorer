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
    to_json(json);

    std::ofstream save(config_path(app_name, settings_filename));
    if(save.good()) {
        save << json.dump(4);
    }
    save.close();
}

//////////////////////////////////////////////////////////////////////

void settings_t::load()
{
    std::ifstream load(config_path(app_name, settings_filename));
    if(load.good()) {
        nlohmann::json json = nlohmann::json::parse(load, nullptr, false, true);
        load.close();
        if(json.is_object()) {
            from_json(json);
        }
    }
}
