//////////////////////////////////////////////////////////////////////

#include <fstream>
#include <nlohmann/json.hpp>

#include "gerber_log.h"
#include "settings.h"
#include "util.h"

LOG_CONTEXT("settings", debug);

//////////////////////////////////////////////////////////////////////

void settings_t::save(std::filesystem::path const &path)
{
    nlohmann::json json;
    to_json(json);

    std::ofstream save(path);
    if(save.good()) {
        save << json.dump(4);
    }
    save.close();
}

//////////////////////////////////////////////////////////////////////

bool settings_t::load(std::filesystem::path const &path)
{
    std::ifstream load(path);
    if(load.good()) {
        nlohmann::json json = nlohmann::json::parse(load, nullptr, false, true);
        load.close();
        if(json.is_object()) {
            from_json(json);
            return true;
        }
    }
    return false;
}
