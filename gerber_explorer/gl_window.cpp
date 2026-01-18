#include <glad/glad.h>
#include <GLFW/glfw3.h>

#undef APIENTRY

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <cmrc/cmrc.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "gerber_log.h"
#include "util.h"
#include "gl_window.h"

#include "assets/matsym_codepoints_utf8.h"

LOG_CONTEXT("gl_window", debug);

CMRC_DECLARE(my_assets);

namespace
{
    using gerber_lib::rect;
    using gerber_lib::vec2d;

    std::string const imgui_ini_filename = config_path("gerber_explorer", "imgui.ini").string();

    void log_gl([[maybe_unused]] GLenum source, [[maybe_unused]] GLenum type, [[maybe_unused]] GLuint id, [[maybe_unused]] GLenum severity,
                [[maybe_unused]] GLsizei length, const GLchar *message, [[maybe_unused]] const void *userParam)
    {
        LOG_INFO("{}:{}", id, message);
    }

    //////////////////////////////////////////////////////////////////////

    void on_glfw_key(GLFWwindow *window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
    {
        ImGuiIO &io = ImGui::GetIO();
        if(io.WantCaptureKeyboard) {
            return;
        }
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_key(key, scancode, action, mods);
    }

    //////////////////////////////////////////////////////////////////////

    void on_glfw_mouse_button(GLFWwindow *window, int button, int action, int mods)
    {
        ImGuiIO &io = ImGui::GetIO();
        if(io.WantCaptureMouse) {
            return;
        }
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_mouse_button(button, action, mods);
    }

    //////////////////////////////////////////////////////////////////////

    void on_glfw_cursor_pos(GLFWwindow *window, double xpos, double ypos)
    {
        ImGuiIO &io = ImGui::GetIO();
        if(io.WantCaptureMouse) {
            return;
        }
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_mouse_move(xpos, ypos);
    }

    void on_glfw_scroll(GLFWwindow *window, double xoffset, double yoffset)
    {
        ImGuiIO &io = ImGui::GetIO();
        if(io.WantCaptureMouse) {
            return;
        }
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_scroll(xoffset, yoffset);
    }

    void on_glfw_size(GLFWwindow *window, int width, int height)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_window_size(width, height);
    }

    void on_glfw_refresh(GLFWwindow *window)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_window_refresh();
    }

    void on_glfw_pos(GLFWwindow *window, int x, int y)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_window_pos(x, y);
    }

    void on_glfw_drop(GLFWwindow *window, int count, char const **paths)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_drop(count, paths);
    }

    void on_glfw_focus(GLFWwindow *window, int focused)
    {
        gl_window *glwindow = static_cast<gl_window *>(glfwGetWindowUserPointer(window));
        glwindow->on_window_focus(focused);
    }

#ifdef _WIN32
    void restore_win32_window(GLFWwindow *window, const gl_window::window_state_t &state)
    {
        HWND hwnd = glfwGetWin32Window(window);
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        HMENU menu = GetMenu(hwnd);
        RECT rect = { 0, 0, state.width, state.height };
        AdjustWindowRectEx(&rect, style, menu != nullptr, exStyle);
        int finalW = rect.right - rect.left;
        int finalH = rect.bottom - rect.top;
        WINDOWPLACEMENT wp = { sizeof(wp) };
        wp.rcNormalPosition = { state.x, state.y, state.x + finalW, state.y + finalH };
        wp.showCmd = state.isMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
        SetWindowPlacement(hwnd, &wp);
    }

    void capture_win32_window(GLFWwindow *window, gl_window::window_state_t &state)
    {
        HWND hwnd = glfwGetWin32Window(window);
        WINDOWPLACEMENT wp = { sizeof(wp) };
        if(GetWindowPlacement(hwnd, &wp)) {
            state.x = wp.rcNormalPosition.left;
            state.y = wp.rcNormalPosition.top;
            DWORD style = GetWindowLong(hwnd, GWL_STYLE);
            DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            HMENU menu = GetMenu(hwnd);
            RECT framePadding = { 0, 0, 0, 0 };
            AdjustWindowRectEx(&framePadding, style, menu != nullptr, exStyle);
            int borderWidth = framePadding.right - framePadding.left;
            int borderHeight = framePadding.bottom - framePadding.top;
            int fullFrameW = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
            int fullFrameH = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            state.width = fullFrameW - borderWidth;
            state.height = fullFrameH - borderHeight;
            state.isMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        }
    }
#endif
}    // namespace

//////////////////////////////////////////////////////////////////////

void gl_window::on_window_size(int w, int h)
{
    if(!glfwGetWindowAttrib(window, GLFW_MAXIMIZED) && !glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
        window_state.width = w;
        window_state.height = h;
    }
}

//////////////////////////////////////////////////////////////////////

void gl_window::on_window_pos(int x, int y)
{
    if(!glfwGetWindowAttrib(window, GLFW_MAXIMIZED) && !glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
        window_state.x = x;
        window_state.y = y;
    }
}

//////////////////////////////////////////////////////////////////////

gl_window::window_state_t gl_window::get_window_state()
{
    window_state_t current_state;
#ifdef _WIN32
    capture_win32_window(window, current_state);
#else
    current_state.isMaximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

    if(!current_state.isMaximized) {
        glfwGetWindowPos(window, &current_state.x, &current_state.y);
        glfwGetWindowSize(window, &current_state.width, &current_state.height);
    } else {
        current_state.x = window_state.x;
        current_state.y = window_state.y;
        current_state.width = window_state.width;
        current_state.height = window_state.height;
    }
#endif
    return current_state;
}

//////////////////////////////////////////////////////////////////////

void gl_window::init()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);    // Required on macOS
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

    window = glfwCreateWindow(800, 600, window_name().c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, on_glfw_key);
    glfwSetMouseButtonCallback(window, on_glfw_mouse_button);
    glfwSetCursorPosCallback(window, on_glfw_cursor_pos);
    glfwSetScrollCallback(window, on_glfw_scroll);
    glfwSetWindowSizeCallback(window, on_glfw_size);
    glfwSetWindowPosCallback(window, on_glfw_pos);
    glfwSetDropCallback(window, on_glfw_drop);
    glfwSetWindowRefreshCallback(window, on_glfw_refresh);
    glfwSetWindowFocusCallback(window, on_glfw_focus);

    glfwMakeContextCurrent(window);

    if(!gladLoadGL()) {
        LOG_ERROR("GLAD LOAD FAILED, Exiting...");
    }

    if(GLAD_GL_ARB_debug_output) {
        GL_CHECK(glDebugMessageCallbackARB(log_gl, nullptr));
        GL_CHECK(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB));
        GL_CHECK(glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, nullptr, GL_FALSE));
        GL_CHECK(glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM_ARB, 0, nullptr, GL_FALSE));
        GL_CHECK(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB));
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    auto fs = cmrc::my_assets::get_filesystem();
    auto roboto_font_file = fs.open("Roboto-Medium.ttf");
    auto matsym_font_file = fs.open("MaterialIcons-Regular.ttf");
    void const* roboto_font_data_ptr = roboto_font_file.begin();
    void const* matsym_font_data_ptr = matsym_font_file.begin();
    size_t roboto_font_data_size = roboto_font_file.size();
    size_t matsym_font_data_size = matsym_font_file.size();

    float fontSize = 18.0f;
    ImFontConfig font_cfg{};
    font_cfg.FontDataOwnedByAtlas = false; // CRITICAL: Tells ImGui NOT to call free()
    io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(roboto_font_data_ptr), static_cast<int>(roboto_font_data_size), fontSize, &font_cfg);

    font_cfg.MergeMode = true;
    font_cfg.PixelSnapH = true;
    font_cfg.GlyphMinAdvanceX = 18.0f;
    font_cfg.GlyphOffset.y = fontSize / 6;
    static const ImWchar icon_ranges[] = { MATSYM_MIN_CODEPOINT, MATSYM_MAX_CODEPOINT, 0 };

    io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(matsym_font_data_ptr), static_cast<int>(matsym_font_data_size), fontSize, &font_cfg);

    io.IniFilename = imgui_ini_filename.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    if(!on_init()) {
        glfwSetWindowShouldClose(window, 1);
        return;
    }
    if(window_state.width == 0 || window_state.height == 0) {
        window_state.width = 800;
        window_state.height = 600;
        window_state.x = 200;
        window_state.y = 200;
    }

    // If the saved position is not on any connected monitor, reset to a safe default on the primary monitor.
    int monitor_count;
    GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
    bool is_visible = false;

    for(int i = 0; i < monitor_count; ++i) {
        int monitorX, monitorY, monitorW, monitorH;
        glfwGetMonitorWorkarea(monitors[i], &monitorX, &monitorY, &monitorW, &monitorH);
        if(window_state.x >= monitorX && window_state.x < (monitorX + monitorW) && window_state.y >= monitorY && window_state.y < (monitorY + monitorH)) {
            is_visible = true;
            break;
        }
    }
    if(!is_visible) {
        int primaryX;
        int primaryY;
        int primaryW;
        int primaryH;
        glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &primaryX, &primaryY, &primaryW, &primaryH);
        window_state.x = primaryX + 100;
        window_state.y = primaryY + 100;
        if(window_state.width > primaryW) {
            window_state.width = primaryW - 200;
        }
        if(window_state.height > primaryH) {
            window_state.height = primaryH - 200;
        }
    }
#ifdef _WIN32
    restore_win32_window(window, window_state);
#else
    glfwSetWindowPos(window, window_state.x, window_state.y);
    glfwSetWindowSize(window, window_state.width, window_state.height);
    if(window_state.isMaximized) {
        glfwPollEvents();
        glfwMaximizeWindow(window);
    }
    glfwShowWindow(window);
#endif
}

//////////////////////////////////////////////////////////////////////

void gl_window::on_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    on_render();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGuiIO &io = ImGui::GetIO();
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow *backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
    glfwSwapBuffers(window);
}

//////////////////////////////////////////////////////////////////////

bool gl_window::update()
{
    glfwWaitEvents();
    if(!glfwWindowShouldClose(window)) {
        on_frame();
        return true;
    }
    on_closed();
    glfwTerminate();
    return false;
}

//////////////////////////////////////////////////////////////////////

void gl_window::on_closed()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
