#pragma once

#include <filesystem>
#include <chrono>

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
void RightAlignButtons(const std::vector<const char *> &labels);
int MsgBox(char const *banner, char const *text, char const *yes_text = "Yes", char const *no_text = "No");

static double get_time()
{
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}
