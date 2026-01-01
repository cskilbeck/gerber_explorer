#pragma once

#include <filesystem>

extern char const *app_name;
extern char const *settings_filename;
extern char const *app_friendly_name;

std::filesystem::path config_path(std::string const &application_name, std::string const &filename);
