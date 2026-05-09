#pragma once

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include "gl_drawer.h"

//////////////////////////////////////////////////////////////////////

struct gl_window
{
    struct window_state_t
    {
        int x, y;
        int width, height;
        bool isMaximized;
    };

    gl_window() = default;
    virtual ~gl_window() = default;

    void init();
    bool update();

    void set_icon(uint8_t const *png_data, size_t png_size) const;

    double frame_start_time{};
    double last_frame_elapsed_time{};

    virtual bool on_init() = 0;
    virtual void on_frame();
    virtual void on_render() = 0;

    virtual void on_closed();

    virtual void on_key(int key, int scancode, int action, int mods)
    {
    }

    virtual void on_mouse_button(int button, int action, int mods)
    {
    }

    virtual void on_mouse_move(double xpos, double ypos)
    {
    }

    virtual void on_scroll(double xoffset, double yoffset)
    {
    }

    virtual void on_drop(int count, const char **paths)
    {
    }

    virtual void on_window_refresh()
    {
    }

    virtual void on_gpu_imgui()
    {
    }

    virtual bool is_idle()
    {
        return true;
    }

    virtual void on_window_focus(int focused)
    {
        window_focused = focused != 0;
    }

    virtual std::string window_name() const
    {
        return "sdl";
    }

    virtual void on_window_size(int w, int h);
    virtual void on_window_pos(int x, int y);

    // Helpers for subclasses that need platform interaction
    void get_window_size(int *w, int *h) const;
    void get_framebuffer_size(int *w, int *h) const;
    void set_should_close();
    void set_cursor_pos(double x, double y);
    void set_input_mode_cursor_normal();
    void set_input_mode_cursor_disabled();
    SDL_Cursor *create_system_cursor(SDL_SystemCursor id);
    void set_cursor(SDL_Cursor *cursor);
    void *get_native_window_handle() const;

    SDL_Window *window{};
    SDL_GLContext gl_context{};
    window_state_t window_state;
    bool window_focused{false};
    bool should_close{false};
    bool use_gpu_backend{false};    // true = SDL_GPU, false = OpenGL
    bool init_complete{false};
    float saved_cursor_x{};
    float saved_cursor_y{};
    bool cursor_hidden{false};

    int64_t frames{};

    window_state_t get_window_state();
};

// SDL uses different key/button/action constants.
// Define compatibility constants so gerber_explorer.cpp can use them.
namespace sdl_compat
{
    // Actions (SDL uses events, not action codes, but we map to these)
    constexpr int ACTION_PRESS = 1;
    constexpr int ACTION_RELEASE = 0;

    // Modifier flags (match SDL_Keymod bits)
    // Note: MOD_CONTROL, MOD_ALT, MOD_SHIFT are #defined by Windows imm.h, so use KMOD_ prefix
    constexpr int KMOD_CTRL_FLAG = SDL_KMOD_CTRL;
    constexpr int KMOD_ALT_FLAG = SDL_KMOD_ALT;
    constexpr int KMOD_SHIFT_FLAG = SDL_KMOD_SHIFT;

    // Mouse buttons
    constexpr int MOUSE_BUTTON_LEFT = SDL_BUTTON_LEFT;
    constexpr int MOUSE_BUTTON_RIGHT = SDL_BUTTON_RIGHT;
    constexpr int MOUSE_BUTTON_MIDDLE = SDL_BUTTON_MIDDLE;

    // Keys (SDL uses SDL_Keycode which are mostly ASCII for letter keys)
    constexpr int KEY_ESCAPE = SDLK_ESCAPE;
    constexpr int KEY_V = SDLK_V;
    constexpr int KEY_F = SDLK_F;
    constexpr int KEY_X = SDLK_X;
    constexpr int KEY_Y = SDLK_Y;
    constexpr int KEY_W = SDLK_W;
    constexpr int KEY_A = SDLK_A;
    constexpr int KEY_E = SDLK_E;
    constexpr int KEY_O = SDLK_O;
    constexpr int KEY_S = SDLK_S;
    constexpr int KEY_L = SDLK_L;
    constexpr int KEY_LEFT_ALT = SDLK_LALT;
    constexpr int KEY_RIGHT_ALT = SDLK_RALT;
}
