#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "gl_drawer.h"
#include "gerber_2d.h"

//////////////////////////////////////////////////////////////////////

struct gl_window
{
    gl_window() = default;
    virtual ~gl_window() = default;

    void init();
    bool update();

    virtual bool on_init() = 0;
    virtual bool on_update() = 0;
    virtual void on_render() = 0;
    virtual void on_closed() = 0;

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

    GLFWwindow *window{};
};
