#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "gerber_log.h"
#include "gl_window.h"

LOG_CONTEXT("gl_window", debug);

namespace
{
    using gerber_lib::gerber_2d::rect;
    using gerber_lib::gerber_2d::vec2d;

    void log_gl([[maybe_unused]] GLenum source, [[maybe_unused]] GLenum type, [[maybe_unused]] GLuint id, [[maybe_unused]] GLenum severity,
                [[maybe_unused]] GLsizei length, const GLchar *message, [[maybe_unused]] const void *userParam)
    {
        LOG_INFO("{}", message);
    }

    //////////////////////////////////////////////////////////////////////

    void on_glfw_key(GLFWwindow *window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_key(key, scancode, action, mods);
    }

    //////////////////////////////////////////////////////////////////////

    void on_glfw_mouse_button(GLFWwindow* window, int button, int action, int mods)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_mouse_button(button, action, mods);

    }

    //////////////////////////////////////////////////////////////////////

    void on_glfw_cursor_pos(GLFWwindow* window, double xpos, double ypos)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_mouse_move(xpos, ypos);
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

void gl_window::init()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfwWindowHint(GLFW_MAXIMIZED, 1);

    window = glfwCreateWindow(800, 600, "GLAD + GLFW", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, on_glfw_key);
    glfwSetMouseButtonCallback(window, on_glfw_mouse_button);
    glfwSetCursorPosCallback(window, on_glfw_cursor_pos);
    glfwMakeContextCurrent(window);

    if(!gladLoadGL()) {
        LOG_ERROR("GLAD LOAD FAILED, Exiting...");
    }

    // GL_CHECK(glDebugMessageCallback(log_gl, nullptr));
    // GL_CHECK(glEnable(GL_DEBUG_OUTPUT));

    // Seems you need this if GL > 3.0 even if vertex array not used directly
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    if(!on_init()) {
        glfwSetWindowShouldClose(window, 1);
    }
}

//////////////////////////////////////////////////////////////////////

bool gl_window::update()
{
    glfwPollEvents();
    if(!glfwWindowShouldClose(window) && on_update()) {
        on_render();
        glfwSwapBuffers(window);
        return true;
    }
    on_closed();
    glfwTerminate();
    return false;
}
