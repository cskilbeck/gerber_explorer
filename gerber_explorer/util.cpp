#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <string>
// #include <unistd.h>

#if defined(__APPLE__)
#include <pwd.h>
#endif

std::optional<std::string> get_env_var(const std::string& key) {
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4996)
#endif
    // ReSharper disable once CppDeprecatedEntity
    const char* val = std::getenv(key.c_str());
#ifdef _WIN32
#pragma warning(pop)
#endif
    if (val == nullptr) {
        return std::nullopt;
    }
    return std::string(val);
}

std::filesystem::path config_path(std::string const &app_name, std::string const &filename)
{
    namespace fs = std::filesystem;

    fs::path base_path;

#if defined(_WIN32)
    // Windows: Use %LOCALAPPDATA%
    auto local_app_data = get_env_var("LOCALAPPDATA");
    if(local_app_data.has_value()) {
        base_path = fs::path(local_app_data.value()) / app_name;
    } else {
        // Fallback if env var is missing
        auto user_profile = get_env_var("USERPROFILE");
        if(!user_profile.has_value()) {
            user_profile = ".";
        }
        base_path = fs::path(user_profile.value()) / "AppData" / "Local" / app_name;
    }

#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support/my_app
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        base_path = fs::path(home) / "Library" / "Application Support" / "my_app";
    } else {
        // Fallback: Check the password database if $HOME is missing
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
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
        base_path = fs::path(xdg_config) / app_name;
    } else {
        const char *home = std::getenv("HOME");
        base_path = home ? (fs::path(home) / ".config" / app_name) : fs::temp_directory_path() / app_name);
    }
#endif

    // Ensure the directory exists before returning the file path
    if(!base_path.empty()) {
        create_directories(base_path);
    }

    return base_path / filename;
}
