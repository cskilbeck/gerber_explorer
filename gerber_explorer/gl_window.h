#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
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

    void set_icon(uint8_t const *png_data, size_t png_size);


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
        return "glfw";
    }

    virtual void on_window_size(int w, int h);
    virtual void on_window_pos(int x, int y);
    GLFWwindow *window{};
    window_state_t window_state;
    bool window_focused{false};

    window_state_t get_window_state();
};
