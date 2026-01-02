#pragma once

#include <filesystem>

//////////////////////////////////////////////////////////////////////

extern char const *app_name;
extern char const *settings_filename;
extern char const *app_friendly_name;

//////////////////////////////////////////////////////////////////////

std::filesystem::path config_path(std::string const &application_name, std::string const &filename);

// ImGui helpers
bool IconCheckbox(const char *label, bool *v, const char *icon_on, const char *icon_off);
bool IconCheckboxTristate(const char *label, int *v, const char *icon_on, const char *icon_off, const char *icon_mixed);
bool IconButton(const char *label, const char *icon);
