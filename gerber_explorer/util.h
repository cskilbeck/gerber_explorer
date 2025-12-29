#pragma once

#include <filesystem>

std::filesystem::path config_path(std::string const &app_name, std::string const &filename);
