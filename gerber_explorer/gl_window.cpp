#include <glad/glad.h>
#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cmrc/cmrc.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdlgpu3.h"

#include "gerber_log.h"
#include "util.h"
#include "gl_window.h"

LOG_CONTEXT("gl_window", debug);

CMRC_DECLARE(my_assets);

namespace
{
    using gerber_lib::rect;
    using gerber_lib::vec2d;

    std::string const imgui_ini_filename = config_path("gerber_explorer", "imgui.ini").string();

    void APIENTRY log_gl([[maybe_unused]] GLenum source, [[maybe_unused]] GLenum type, [[maybe_unused]] GLuint id, [[maybe_unused]] GLenum severity,
                         [[maybe_unused]] GLsizei length, const GLchar *message, [[maybe_unused]] const void *userParam)
    {
        LOG_INFO("{}:{}", id, message);
    }
}    // namespace

//////////////////////////////////////////////////////////////////////

void gl_window::set_icon(uint8_t const *png_data, size_t png_size) const
{
    int w, h;
    uint8_t *pixels = stbi_load_from_memory(png_data, (int)png_size, &w, &h, nullptr, 4);
    if(pixels) {
        SDL_Surface *surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, pixels, w * 4);
        if(surface) {
            SDL_SetWindowIcon(window, surface);
            SDL_DestroySurface(surface);
        }
        stbi_image_free(pixels);
    }
}

//////////////////////////////////////////////////////////////////////

void gl_window::on_window_size(int w, int h)
{
    if(!(SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))) {
        window_state.width = w;
        window_state.height = h;
    }
}

//////////////////////////////////////////////////////////////////////

void gl_window::on_window_pos(int x, int y)
{
    if(!(SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))) {
        window_state.x = x;
        window_state.y = y;
    }
}

//////////////////////////////////////////////////////////////////////

gl_window::window_state_t gl_window::get_window_state()
{
    window_state_t current_state;
    Uint32 flags = SDL_GetWindowFlags(window);
    current_state.isMaximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;

    if(!current_state.isMaximized) {
        SDL_GetWindowPosition(window, &current_state.x, &current_state.y);
        SDL_GetWindowSize(window, &current_state.width, &current_state.height);
    } else {
        current_state.x = window_state.x;
        current_state.y = window_state.y;
        current_state.width = window_state.width;
        current_state.height = window_state.height;
    }
    return current_state;
}

//////////////////////////////////////////////////////////////////////
// Helper methods for subclasses

void gl_window::get_window_size(int *w, int *h) const
{
    SDL_GetWindowSize(window, w, h);
}

void gl_window::get_framebuffer_size(int *w, int *h) const
{
    SDL_GetWindowSizeInPixels(window, w, h);
}

void gl_window::set_should_close()
{
    should_close = true;
}

void gl_window::set_cursor_pos(double x, double y)
{
    SDL_WarpMouseInWindow(window, (float)x, (float)y);
}

void gl_window::set_input_mode_cursor_normal()
{
    if(cursor_hidden) {
        SDL_SetWindowRelativeMouseMode(window, false);
        SDL_WarpMouseInWindow(window, saved_cursor_x, saved_cursor_y);
        SDL_ShowCursor();
        cursor_hidden = false;
    }
}

void gl_window::set_input_mode_cursor_disabled()
{
    if(!cursor_hidden) {
        SDL_GetMouseState(&saved_cursor_x, &saved_cursor_y);
        SDL_SetWindowRelativeMouseMode(window, true);
        cursor_hidden = true;
    }
}

SDL_Cursor *gl_window::create_system_cursor(SDL_SystemCursor id)
{
    return SDL_CreateSystemCursor(id);
}

void gl_window::set_cursor(SDL_Cursor *cursor)
{
    SDL_SetCursor(cursor);
}

void *gl_window::get_native_window_handle() const
{
#ifdef _WIN32
    return (void *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    return nullptr;
#endif
}

//////////////////////////////////////////////////////////////////////

void gl_window::init()
{
    SDL_Init(SDL_INIT_VIDEO);

    if(use_gpu_backend) {
        // SDL_GPU mode - no GL context needed, just create a plain window
        window = SDL_CreateWindow(window_name().c_str(), 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    } else {
        // OpenGL mode
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

        window = SDL_CreateWindow(window_name().c_str(), 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);

        if(!gladLoadGL()) {
            LOG_ERROR("GLAD LOAD FAILED, Exiting...");
        }

        if(GLAD_GL_ARB_debug_output) {
            GL_CHECK(glDebugMessageCallbackARB(log_gl, nullptr));
            GL_CHECK(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB));
            GL_CHECK(glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, nullptr, GL_FALSE));
            GL_CHECK(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB));
        }
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    auto fs = cmrc::my_assets::get_filesystem();
    auto roboto_font_file = fs.open("Roboto-Medium.ttf");
    auto matsym_font_file = fs.open("MaterialIcons-Regular.ttf");
    void const *roboto_font_data_ptr = roboto_font_file.begin();
    void const *matsym_font_data_ptr = matsym_font_file.begin();
    size_t roboto_font_data_size = roboto_font_file.size();
    size_t matsym_font_data_size = matsym_font_file.size();

    float fontSize = 18.0f;
    ImFontConfig font_cfg{};
    font_cfg.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(roboto_font_data_ptr), static_cast<int>(roboto_font_data_size), fontSize, &font_cfg);

    font_cfg.MergeMode = true;
    font_cfg.PixelSnapH = true;
    font_cfg.GlyphMinAdvanceX = 18.0f;
    font_cfg.GlyphOffset.y = fontSize / 6;

    io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(matsym_font_data_ptr), static_cast<int>(matsym_font_data_size), fontSize, &font_cfg);

    io.IniFilename = imgui_ini_filename.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if(!use_gpu_backend) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }
    ImGui::StyleColorsDark();
    if(use_gpu_backend) {
        // ImGui platform + renderer are initialized later in on_init() after GPU device is created
    } else {
        ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    if(!on_init()) {
        should_close = true;
        return;
    }
    if(window_state.width == 0 || window_state.height == 0) {
        window_state.width = 800;
        window_state.height = 600;
        window_state.x = 200;
        window_state.y = 200;
    }

    // If the saved position is not on any connected monitor, reset to a safe default
    int display_count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
    bool is_visible = false;

    if(displays) {
        for(int i = 0; i < display_count; ++i) {
            SDL_Rect usable;
            if(SDL_GetDisplayUsableBounds(displays[i], &usable)) {
                if(window_state.x >= usable.x && window_state.x < (usable.x + usable.w) && window_state.y >= usable.y &&
                   window_state.y < (usable.y + usable.h)) {
                    is_visible = true;
                    break;
                }
            }
        }
        SDL_free(displays);
    }

    if(!is_visible) {
        SDL_DisplayID primary = SDL_GetPrimaryDisplay();
        SDL_Rect usable;
        if(SDL_GetDisplayUsableBounds(primary, &usable)) {
            window_state.x = usable.x + 100;
            window_state.y = usable.y + 100;
            if(window_state.width > usable.w) {
                window_state.width = usable.w - 200;
            }
            if(window_state.height > usable.h) {
                window_state.height = usable.h - 200;
            }
        }
    }

    SDL_SetWindowPosition(window, window_state.x, window_state.y);
    SDL_SetWindowSize(window, window_state.width, window_state.height);
    if(window_state.isMaximized) {
        SDL_MaximizeWindow(window);
    }
    SDL_ShowWindow(window);
    init_complete = true;
}

//////////////////////////////////////////////////////////////////////

void gl_window::on_frame()
{
    if(!init_complete) {
        return;
    }

    if(use_gpu_backend) {
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        on_render();    // calls gpu_render() which submits gerber rendering, then ui()
        ImGui::Render();
        on_gpu_imgui();    // virtual - submits ImGui draw data via GPU
    } else {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        on_render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ImGuiIO &io = ImGui::GetIO();
        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_GLContext backup_context = SDL_GL_GetCurrentContext();
            SDL_Window *backup_window = SDL_GL_GetCurrentWindow();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_window, backup_context);
        }
        SDL_GL_SwapWindow(window);
    }
    frames += 1;
}

//////////////////////////////////////////////////////////////////////

bool gl_window::update()
{
    if(is_idle()) {
        SDL_WaitEvent(nullptr);
    }

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch(event.type) {

        case SDL_EVENT_QUIT:
            should_close = true;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if(event.window.windowID == SDL_GetWindowID(window)) {
                should_close = true;
            }
            break;

        case SDL_EVENT_WINDOW_RESIZED: {
            if(event.window.windowID == SDL_GetWindowID(window)) {
                on_window_size(event.window.data1, event.window.data2);
            }
        } break;

        case SDL_EVENT_WINDOW_MOVED: {
            if(event.window.windowID == SDL_GetWindowID(window)) {
                on_window_pos(event.window.data1, event.window.data2);
            }
        } break;

        case SDL_EVENT_WINDOW_EXPOSED: {
            if(event.window.windowID == SDL_GetWindowID(window)) {
                on_window_refresh();
            }
        } break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if(event.window.windowID == SDL_GetWindowID(window)) {
                on_window_focus(1);
            }
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if(event.window.windowID == SDL_GetWindowID(window)) {
                on_window_focus(0);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            ImGuiIO &io = ImGui::GetIO();
            if(io.WantCaptureKeyboard) {
                break;
            }
            int action = (event.type == SDL_EVENT_KEY_DOWN) ? sdl_compat::ACTION_PRESS : sdl_compat::ACTION_RELEASE;
            int mods = 0;
            if(event.key.mod & SDL_KMOD_CTRL)
                mods |= sdl_compat::KMOD_CTRL_FLAG;
            if(event.key.mod & SDL_KMOD_ALT)
                mods |= sdl_compat::KMOD_ALT_FLAG;
            if(event.key.mod & SDL_KMOD_SHIFT)
                mods |= sdl_compat::KMOD_SHIFT_FLAG;
            on_key(event.key.key, event.key.scancode, action, mods);
        } break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            ImGuiIO &io = ImGui::GetIO();
            if(io.WantCaptureMouse) {
                break;
            }
            int action = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? sdl_compat::ACTION_PRESS : sdl_compat::ACTION_RELEASE;
            int mods = SDL_GetModState();
            on_mouse_button(event.button.button, action, mods);
        } break;

        case SDL_EVENT_MOUSE_MOTION: {
            ImGuiIO &io = ImGui::GetIO();
            if(io.WantCaptureMouse) {
                break;
            }
            on_mouse_move(event.motion.x, event.motion.y);
        } break;

        case SDL_EVENT_MOUSE_WHEEL: {
            ImGuiIO &io = ImGui::GetIO();
            if(io.WantCaptureMouse) {
                break;
            }
            on_scroll(event.wheel.x, event.wheel.y);
        } break;

        case SDL_EVENT_DROP_FILE: {
            char const *path = event.drop.data;
            if(path) {
                on_drop(1, &path);
            }
        } break;
        }
    }

    if(should_close) {
        on_closed();
        if(!use_gpu_backend && gl_context) {
            SDL_GL_DestroyContext(gl_context);
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    frame_start_time = get_time();
    on_frame();
    last_frame_elapsed_time = get_time() - frame_start_time;
    return true;
}

//////////////////////////////////////////////////////////////////////

void gl_window::on_closed()
{
    if(use_gpu_backend) {
        ImGui_ImplSDLGPU3_Shutdown();
    } else {
        ImGui_ImplOpenGL3_Shutdown();
    }
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
