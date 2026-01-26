#define _CRT_SECURE_NO_WARNINGS

#include <filesystem>
#include <cstdlib>
#include <string>
#include <optional>
#include <vector>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

//////////////////////////////////////////////////////////////////////

char const *app_name{ "gerber_explorer" };
char const *app_friendly_name{ "Gerber Viewer" };
char const *settings_filename{ "settings.json" };

#if defined(__APPLE__)
#include <pwd.h>
#endif

//////////////////////////////////////////////////////////////////////

std::optional<std::string> get_env_var(const std::string &key)
{
#if defined(MSVC)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    // ReSharper disable once CppDeprecatedEntity
    const char *val = std::getenv(key.c_str());
#if defined(MSVC)
#pragma warning(pop)
#endif
    if(val == nullptr) {
        return std::nullopt;
    }
    return std::string(val);
}

//////////////////////////////////////////////////////////////////////

std::filesystem::path config_path(std::string const &application_name, std::string const &filename)
{
    namespace fs = std::filesystem;

    fs::path base_path;

#if defined(_WIN32)
    // Windows: Use %LOCALAPPDATA%
    auto local_app_data = get_env_var("LOCALAPPDATA");
    if(local_app_data.has_value()) {
        base_path = fs::path(local_app_data.value()) / application_name;
    } else {
        // Fallback if env var is missing
        auto user_profile = get_env_var("USERPROFILE");
        if(user_profile.has_value()) {
            base_path = fs::path(user_profile.value()) / "AppData" / "Local" / application_name;
        } else {
            base_path = fs::path(".") / application_name;
        }
    }

#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support/my_app
    const char *home = std::getenv("HOME");
    if(home && home[0] != '\0') {
        base_path = fs::path(home) / "Library" / "Application Support" / "my_app";
    } else {
        // Fallback: Check the password database if $HOME is missing
        struct passwd *pw = getpwuid(getuid());
        if(pw && pw->pw_dir) {
            base_path = fs::path(pw->pw_dir) / "Library" / "Application Support" / "my_app";
        } else {
            // Last resort: temporary directory
            base_path = fs::temp_directory_path() / "my_app";
        }
    }

#else
    // Linux/Unix: Respect XDG_CONFIG_HOME, fallback to ~/.config
    const char *xdg_config = std::getenv("XDG_CONFIG_HOME");
    if(xdg_config && xdg_config[0] != '\0') {
        base_path = fs::path(xdg_config) / application_name;
    } else {
        const char *home = std::getenv("HOME");
        base_path = home ? (fs::path(home) / ".config" / application_name) : fs::temp_directory_path() / application_name);
    }
#endif

    // Ensure the directory exists before returning the file path
    if(!base_path.empty()) {
        create_directories(base_path);
    }

    return base_path / filename;
}

//////////////////////////////////////////////////////////////////////

bool IconCheckbox(const char *label, bool *v, const char *icon_on, const char *icon_off)
{
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if(window->SkipItems) {
        return false;
    }

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

    const float square_sz = ImGui::GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    // Total bounding box (Icon square + Padding + Label text)
    const ImRect total_bb(
        pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if(!ImGui::ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if(pressed) {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }

    // 1. Render Background Frame
    const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderFrame(pos, pos + ImVec2(square_sz, square_sz), col, true, style.FrameRounding);

    // 2. Render the Icon
    const char *icon = *v ? icon_on : icon_off;
    if(*v || icon_off) {    // Only draw if checked, or if an "off" icon is provided
        ImVec2 icon_size = ImGui::CalcTextSize(icon);
        // Center the icon in the square
        ImVec2 icon_pos = pos + ImVec2((square_sz - icon_size.x) * 0.5f, (square_sz - icon_size.y) * 0.5f);
        window->DrawList->AddText(icon_pos, ImGui::GetColorU32(ImGuiCol_Text), icon);
    }

    // 3. Render Label
    if(label_size.x > 0.0f) {
        ImGui::RenderText(pos + ImVec2(square_sz + style.ItemInnerSpacing.x, style.FramePadding.y), label);
    }

    return pressed;
}

//////////////////////////////////////////////////////////////////////

bool IconCheckboxTristate(const char *label, int *v, const char *icon_on, const char *icon_off, const char *icon_mixed)
{
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if(window->SkipItems) {
        return false;
    }

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

    const float square_sz = ImGui::GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(
        pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if(!ImGui::ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

    if(pressed) {
        // Simple toggle logic: If Mixed or Off -> turn On. If On -> turn Off.
        *v += 1;
        if(*v >= 3) {
            *v = 0;
        }
        ImGui::MarkItemEdited(id);
    }

    // 1. Render Background Frame
    const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderFrame(pos, pos + ImVec2(square_sz, square_sz), col, true, style.FrameRounding);

    // 2. Icon Selection Logic
    const char *icon = icon_off;
    if(*v == 1)
        icon = icon_on;
    else if(*v == 2)
        icon = icon_mixed;

    if(icon) {
        ImVec2 icon_size = ImGui::CalcTextSize(icon);
        ImVec2 icon_pos = pos + ImVec2((square_sz - icon_size.x) * 0.5f, (square_sz - icon_size.y) * 0.5f);

        // Optional: Dim the color if in mixed state to match ImGui's native feel
        ImU32 icon_col = ImGui::GetColorU32(ImGuiCol_Text);
        window->DrawList->AddText(icon_pos, icon_col, icon);
    }

    // 3. Render Label
    if(label_size.x > 0.0f) {
        ImGui::RenderText(pos + ImVec2(square_sz + style.ItemInnerSpacing.x, style.FramePadding.y), label);
    }

    return pressed;
}

//////////////////////////////////////////////////////////////////////

bool IconButton(const char *label, const char *icon)
{
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if(window->SkipItems)
        return false;

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

    const float square_sz = ImGui::GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    // Total bounding box (Icon square + Padding + Label text)
    const ImRect total_bb(
        pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if(!ImGui::ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

    // 1. Render Background Frame
    const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderFrame(pos, pos + ImVec2(square_sz, square_sz), col, true, style.FrameRounding);

    // 3. Render Label
    if(label_size.x > 0.0f) {
        ImGui::RenderText(pos + ImVec2(square_sz + style.ItemInnerSpacing.x, style.FramePadding.y), label);
    }

    // 2. Render the Icon
    ImVec2 icon_size = ImGui::CalcTextSize(icon);
    // Center the icon in the square
    ImVec2 icon_pos = pos + ImVec2((square_sz - icon_size.x) * 0.5f, (square_sz - icon_size.y) * 0.5f);
    window->DrawList->AddText(icon_pos, ImGui::GetColorU32(ImGuiCol_Text), icon);


    return pressed;
}

//////////////////////////////////////////////////////////////////////

void RightAlignButtons(const std::vector<const char *> &labels) {
    float totalWidth = 0.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    for (auto label : labels) {
        totalWidth += ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    }
    totalWidth += spacing * (labels.size() - 1);
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - totalWidth - ImGui::GetStyle().WindowPadding.x);
}

//////////////////////////////////////////////////////////////////////

int MsgBox(char const *banner, char const *text, char const *yes_text = "Yes", char const *no_text = "No")
{
    int rc = 0;
    ImVec2 text_size = ImGui::CalcTextSize(text, nullptr, false, ImGui::GetWindowWidth() / 3.0f);
    float pad = ImGui::GetFontSize() * 0.33333f;
    float w = std::max(ImGui::GetWindowWidth() / 6.0f, text_size.x + pad * 8 + 1);
    ImVec2 min_size(w, -1.0f);
    ImVec2 max_size(w, -1.0f);
    ImGui::SetNextWindowSizeConstraints(min_size, max_size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad * 4, pad * 2));
    if(ImGui::BeginPopupModal(banner, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::TextWrapped("%s", text);
        ImGui::Dummy(ImVec2(0.0f, pad));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, pad));
        RightAlignButtons({yes_text, no_text});
        if(ImGui::Button(yes_text)) {
            rc = 1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if(ImGui::Button(no_text)) {
            rc = 2;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(1);
    return rc;
}
