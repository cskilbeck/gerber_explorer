//////////////////////////////////////////////////////////////////////

#include "gerber_log.h"
#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_util.h"

#include <vector>
#include <thread>

#include <filesystem>

#include <gl/GL.h>
#include "Wglext.h"
#include "glcorearb.h"

#define WIN32_LEAN_AND_MEAN
// #define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <Commdlg.h>

#include "gl_base.h"
#include "gl_matrix.h"
#include "gl_window.h"
#include "gl_colors.h"
#include "gl_functions.h"

LOG_CONTEXT("gl_window", info);

namespace
{
    //////////////////////////////////////////////////////////////////////

    auto constexpr WM_SHOW_OPEN_FILE_DIALOG = WM_USER;
    auto constexpr WM_GERBER_WAS_LOADED = WM_USER + 1;
    auto constexpr WM_FIT_TO_WINDOW = WM_USER + 2;

    using vec2d = gerber_lib::gerber_2d::vec2d;

    using namespace gerber_lib;
    using namespace gerber_3d;

    long long const zoom_lerp_time_ms = 700;

    double const drag_select_offset_start_distance = 16;

    uint32_t layer_colors[] = { gl_color::yellow,           gl_color::green, gl_color::dark_cyan, gl_color::lime_green, gl_color::antique_white,
                                gl_color::corn_flower_blue, gl_color::gold };

    uint32_t layer_color = gl_color::red;

    //////////////////////////////////////////////////////////////////////
    // make a rectangle have a certain aspect ratio by shrinking or expanding it

    enum aspect_ratio_correction
    {
        aspect_shrink,
        aspect_expand,
    };

    rect correct_aspect_ratio(double new_aspect_ratio, rect const &r, aspect_ratio_correction correction)
    {
        bool dir = r.aspect_ratio() > new_aspect_ratio;
        if(correction == aspect_expand) {
            dir = !dir;
        }
        vec2d n = r.size().scale(0.5);
        if(dir) {
            n.x = n.y * new_aspect_ratio;
        } else {
            n.y = n.x / new_aspect_ratio;
        }
        vec2d center = r.mid_point();
        return rect{ center.subtract(n), center.add(n) };
    }

    //////////////////////////////////////////////////////////////////////
    // make a matrix which maps window rect to world coordinates
    // if aspect ratio(view_rect) != aspect_ratio(window_rect), there will be distortion

    void make_world_to_window_transform(gl_matrix result, rect const &window, rect const &view)
    {
        gl_matrix scale;
        gl_matrix origin;

        make_scale(scale, static_cast<float>(window.width() / view.width()), static_cast<float>(window.height() / view.height()));
        make_translate(origin, -static_cast<float>(view.min_pos.x), -static_cast<float>(view.min_pos.y));
        matrix_multiply(scale, origin, result);
    }

    //////////////////////////////////////////////////////////////////////

    bool is_mouse_message(UINT message)
    {
        return message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
    }

    //////////////////////////////////////////////////////////////////////

    bool is_keyboard_message(UINT message)
    {
        return message >= WM_KEYFIRST && message <= WM_KEYLAST;
    }

    //////////////////////////////////////////////////////////////////////

    vec2d pos_from_lparam(LPARAM lParam)
    {
        return vec2d{ static_cast<double>(GET_X_LPARAM(lParam)), static_cast<double>(GET_Y_LPARAM(lParam)) };
    }

}    // namespace

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    void gl_window::show_3d_view()
    {
    }

    //////////////////////////////////////////////////////////////////////

    bool gl_window::get_open_filenames(std::vector<std::string> &filenames)
    {
        // open a file name
        char filename[MAX_PATH * 20] = {};
        OPENFILENAME ofn{};
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = filename;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = static_cast<DWORD>(gerber_util::array_length(filename));
        ofn.lpstrFilter = "All files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

        if(GetOpenFileNameA(&ofn)) {
            int num_files = 1;
            char const *n = ofn.lpstrFile;
            std::string first_name{ n };
            n += strlen(n) + 1;
            while(*n) {
                filenames.push_back(std::format("{}\\{}", first_name, std::string{ n }));
                n += strlen(n) + 1;
                num_files += 1;
            }
            if(num_files == 1) {
                filenames.push_back(first_name);
            }
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // must pass the names by value here!

    void gl_window::load_gerber_files(std::vector<std::string> filenames)
    {
        // std::thread([this, filenames] {
        //     std::vector<std::thread> threads;
        //     std::vector<gl_drawer *> drawers;
        //     drawers.resize(100);
        //     int index = 0;
        //     for(auto const &s : filenames) {
        //         LOG_DEBUG("LOADING {}", s);
        //         threads.emplace_back(
        //             // ReSharper disable once CppPassValueParameterByConstReference
        //             [&](std::string filename, int idx) {
        //                 gerber *g = new gerber();
        //                 if(g->parse_file(filename.c_str()) == ok) {
        //                     gl_drawer *drawer = new gl_drawer();
        //                     drawers[idx] = drawer;
        //                     drawer->program = &layer_program;
        //                     drawer->set_gerber(g);
        //                 }
        //             },
        //             s, index);
        //         if(++index == drawers.size()) {
        //             LOG_WARNING("Can't load > 100 files at once! Stopping...");
        //             break;
        //         }
        //     }
        //     LOG_DEBUG("{} threads", threads.size());
        //     int id = 0;
        //     for(auto &thread : threads) {
        //         LOG_DEBUG("Joining thread {}", id++);
        //         thread.join();
        //     }
        //     threads.clear();
        //     for(int i = 0; i < index; ++i) {
        //         gl_drawer *d = drawers[i];
        //         PostMessage(hwnd, WM_GERBER_WAS_LOADED, 0, (LPARAM)d);
        //     }
        //     PostMessage(hwnd, WM_FIT_TO_WINDOW, 0, 0);
        // }).detach();
    }

    //////////////////////////////////////////////////////////////////////

    vec2d gl_window::world_pos_from_window_pos(vec2d const &p) const
    {
        vec2d scale = view_rect.size().divide(window_size);
        return vec2d{ p.x, window_size.y - p.y }.multiply(scale).add(view_rect.min_pos);
    }

    //////////////////////////////////////////////////////////////////////

    vec2d gl_window::window_pos_from_world_pos(vec2d const &p) const
    {
        vec2d scale = window_size.divide(view_rect.size());
        vec2d pos = p.subtract(view_rect.min_pos).multiply(scale);
        return { pos.x, window_size.y - pos.y };
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::fit_to_window()
    {
        // if(selected_layer != nullptr) {
        //     zoom_to_rect(selected_layer->layer->gerber_file->image.info.extent);
        // } else {
        //     rect all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
        //     for(auto layer : layers) {
        //         all = all.union_with(layer->layer->gerber_file->image.info.extent);
        //     }
        //     zoom_to_rect(all);
        // }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::zoom_to_rect(rect const &zoom_rect, double border_ratio)
    {
        if(window_rect.width() == 0 || window_rect.height() == 0) {
            return;
        }
        rect new_rect = correct_aspect_ratio(window_rect.aspect_ratio(), zoom_rect, aspect_expand);
        vec2d mid = new_rect.mid_point();
        vec2d siz = new_rect.size().scale(border_ratio / 2);
        target_view_rect = { mid.subtract(siz), mid.add(siz) };
        source_view_rect = view_rect;
        target_view_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(zoom_lerp_time_ms);
        zoom_anim = true;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::zoom_image(vec2d const &pos, double zoom_scale)
    {
        // normalized position within view_rect
        vec2d zoom_pos = vec2d{ static_cast<double>(pos.x), window_size.y - static_cast<double>(pos.y) }.divide(window_size);

        // scaled view_rect size
        vec2d new_size{ view_rect.width() / zoom_scale, view_rect.height() / zoom_scale };

        // world position of pos
        vec2d p = view_rect.min_pos.add(zoom_pos.multiply(view_rect.size()));

        // new rectangle offset from world position
        vec2d bottom_left = p.subtract(new_size.multiply(zoom_pos));
        vec2d top_right = bottom_left.add(new_size);
        view_rect = { bottom_left, top_right };
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::update_view_rect()
    {
        if(zoom_anim) {

            auto lerp = [](double d) {
                double p = 10;
                double x = d;
                double x2 = pow(x, p - 1);
                double x1 = x2 * x;    // pow(x, p)
                return 1 - (p * x2 - (p - 1) * x1);
            };

            auto now = std::chrono::high_resolution_clock::now();
            auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(target_view_time - now).count();
            if(remaining_ms < 1) {
                remaining_ms = 0;
            }
            double t = static_cast<double>(remaining_ms) / static_cast<double>(zoom_lerp_time_ms);
            double d = lerp(t);
            vec2d dmin = target_view_rect.min_pos.subtract(source_view_rect.min_pos).scale(d);
            vec2d dmax = target_view_rect.max_pos.subtract(source_view_rect.max_pos).scale(d);
            view_rect = { source_view_rect.min_pos.add(dmin), source_view_rect.max_pos.add(dmax) };
            vec2d wv = window_pos_from_world_pos(view_rect.min_pos);
            vec2d tv = window_pos_from_world_pos(target_view_rect.min_pos);
            if(wv.subtract(tv).length() <= 1) {
                LOG_DEBUG("View zoom complete");
                view_rect = target_view_rect;
                zoom_anim = false;
            }
        }
    }

    // //////////////////////////////////////////////////////////////////////
    //
    // void gl_window::select_layer(gerber_layer *l)
    // {
    //     if(selected_layer != nullptr) {
    //         selected_layer->selected = false;
    //     }
    //     if(l != nullptr) {
    //         l->selected = true;
    //     }
    //     selected_layer = l;
    // }
    //
    // //////////////////////////////////////////////////////////////////////
    //
    // void gl_window::gerber_layer::draw(bool wireframe, float outline_thickness)
    // {
    //     layer->draw(fill, outline, wireframe, outline_thickness);
    // }
    //
    //////////////////////////////////////////////////////////////////////

    void gl_window::set_mouse_mode(mouse_drag_action action, vec2d const &pos)
    {
        auto show_mouse = [] {
            while(ShowCursor(true) < 0) {
            }
        };

        auto hide_mouse = [] {
            while(ShowCursor(false) >= 0) {
            }
        };

        auto begin = [&] {
            zoom_anim = false;
            show_mouse();
            SetCapture(hwnd);
        };

        switch(action) {

        case mouse_drag_none:
            zoom_anim = mouse_mode == mouse_drag_zoom_select;
            show_mouse();
            ReleaseCapture();
            break;

        case mouse_drag_pan:
            drag_mouse_start_pos = pos;
            begin();
            break;

        case mouse_drag_zoom:
            zoom_anim = false;
            hide_mouse();
            SetCapture(hwnd);
            drag_mouse_start_pos = pos;
            break;

        case mouse_drag_zoom_select:
            begin();
            drag_mouse_start_pos = pos;
            drag_rect = {};
            break;

        case mouse_drag_maybe_select: {
            begin();
            drag_mouse_start_pos = pos;
        } break;

        case mouse_drag_select:
            begin();
            drag_mouse_cur_pos = pos;
            drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
            break;
        }
        mouse_mode = action;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::on_mouse_move(vec2d const &mouse_pos)
    {
        switch(mouse_mode) {

        case mouse_drag_pan: {
            vec2d new_mouse_pos = world_pos_from_window_pos(mouse_pos);
            vec2d old_mouse_pos = world_pos_from_window_pos(drag_mouse_start_pos);
            view_rect = view_rect.offset(new_mouse_pos.subtract(old_mouse_pos).negate());
            drag_mouse_start_pos = mouse_pos;
        } break;

        case mouse_drag_zoom: {
            vec2d d = mouse_pos.subtract(drag_mouse_start_pos);
            zoom_image(drag_mouse_start_pos, 1.0 + (d.x - d.y) * 0.01);
            drag_mouse_cur_pos = mouse_pos;
            POINT screen_pos{ static_cast<long>(drag_mouse_start_pos.x), static_cast<long>(drag_mouse_start_pos.y) };
            ClientToScreen(hwnd, &screen_pos);
            SetCursorPos(screen_pos.x, screen_pos.y);
        } break;

        case mouse_drag_zoom_select: {
            drag_mouse_cur_pos = mouse_pos;
            if(drag_mouse_cur_pos.subtract(drag_mouse_start_pos).length() > 4) {
                drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos }.normalize();
            }
        } break;

        case mouse_drag_maybe_select: {
            if(mouse_pos.subtract(drag_mouse_start_pos).length() > drag_select_offset_start_distance) {
                set_mouse_mode(mouse_drag_select, mouse_pos);
                mouse_world_pos = world_pos_from_window_pos(mouse_pos);
            }
        } break;

        case mouse_drag_select: {
            drag_mouse_cur_pos = mouse_pos;
            drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
            // select_entities(drag_rect, (GetKeyState(VK_SHIFT) & 0x8000) == 0);
        } break;

        case mouse_drag_none: {
        } break;

        default:
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK gl_window::wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if(message == WM_CREATE) {
            LPCREATESTRUCTA c = reinterpret_cast<LPCREATESTRUCTA>(lParam);
            SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c->lpCreateParams));
        }
        gl_window *d = reinterpret_cast<gl_window *>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if(d != nullptr) {
            return d->wnd_proc(message, wParam, lParam);
        }
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK gl_window::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        LRESULT result = 0;

        switch(message) {

        case WM_SIZING: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            draw(rc.right, rc.bottom);
            ValidateRect(hwnd, nullptr);
        } break;

        case WM_SHOWWINDOW:
            break;

        case WM_SIZE: {
            if(IsWindowVisible(hwnd)) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                vec2d new_window_size = { static_cast<double>(rc.right), static_cast<double>(rc.bottom) };
                LOG_DEBUG("New window size: {}", new_window_size);
                if(window_size.x == 0 || window_size.y == 0) {
                    window_size = new_window_size;
                    window_rect = { { 0, 0 }, window_size };
                    view_rect = window_rect.offset(window_size.scale(-0.5));
                } else {
                    vec2d scale_factor = new_window_size.divide(window_size);
                    window_size = new_window_size;
                    window_rect = { { 0, 0 }, window_size };
                    vec2d new_view_size = view_rect.size().multiply(scale_factor);
                    view_rect.max_pos = view_rect.min_pos.add(new_view_size);
                }
                // if(!window_size_valid && !layers.empty()) {
                //     zoom_to_rect(layers.front()->layer->gerber_file->image.info.extent);
                // }
                window_size_valid = true;
            }
            ValidateRect(hwnd, nullptr);
        } break;

        case WM_MOUSEWHEEL: {
            double scale_factor = (static_cast<int16_t>((HIWORD(wParam))) > 0) ? 1.1 : 0.9;
            POINT pos = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pos);
            zoom_image(vec2d{ static_cast<double>(pos.x), static_cast<double>(pos.y) }, scale_factor);
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_LBUTTONDOWN: {
            if((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                set_mouse_mode(mouse_drag_zoom_select, pos_from_lparam(lParam));
            } else {
                set_mouse_mode(mouse_drag_maybe_select, pos_from_lparam(lParam));
            }
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_LBUTTONUP: {
            if(mouse_mode == mouse_drag_zoom_select) {
                rect drag_rect_corrected = correct_aspect_ratio(window_rect.aspect_ratio(), drag_rect, aspect_expand);
                vec2d mn = drag_rect_corrected.min_pos;
                vec2d mx = drag_rect_corrected.max_pos;
                rect d = rect{ mn, mx }.normalize();
                if(d.width() > 2 && d.height() > 2) {
                    zoom_to_rect({ world_pos_from_window_pos(vec2d{ mn.x, mx.y }), world_pos_from_window_pos(vec2d{ mx.x, mn.y }) });
                }
            }
            set_mouse_mode(mouse_drag_none, {});
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONDOWN:
            set_mouse_mode(mouse_drag_zoom, pos_from_lparam(lParam));
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONUP:
            set_mouse_mode(mouse_drag_none, {});
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONDOWN:
            set_mouse_mode(mouse_drag_pan, pos_from_lparam(lParam));
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONUP:
            set_mouse_mode(mouse_drag_none, {});
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MOUSEMOVE:

            on_mouse_move(pos_from_lparam(lParam));
            break;

        case WM_KEYDOWN:

            switch(wParam) {

            case VK_ESCAPE:
                DestroyWindow(hwnd);
                break;

            case '3':
                show_3d_view();
                break;

            default:
                break;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            wglMakeCurrent(window_dc, nullptr);
            wglDeleteContext(render_context);
            render_context = nullptr;
            ReleaseDC(hwnd, window_dc);
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            ValidateRect(hwnd, nullptr);
            break;

        case WM_SHOW_OPEN_FILE_DIALOG: {
            std::vector<std::string> filenames;
            if(get_open_filenames(filenames)) {
                load_gerber_files(filenames);
            }
        } break;

        case WM_GERBER_WAS_LOADED: {
            // gl_drawer *drawer = (gl_drawer *)lParam;
            // if(drawer != nullptr) {
            //     drawer->on_finished_loading();
            //     gerber_layer *layer = new gerber_layer();
            //     layer->layer = drawer;
            //     layer->fill_color = layer_colors[layers.size() % gerber_util::array_length(layer_colors)];
            //     layer->clear_color = gl_color::cyan;
            //     layer->outline_color = gl_color::magenta;
            //     layer->outline = true;
            //     layer->filename = std::filesystem::path(drawer->gerber_file->filename).filename().string();
            //     layers.push_back(layer);
            // }
        } break;

        case WM_FIT_TO_WINDOW: {
            fit_to_window();
        } break;

        default:
            result = DefWindowProcA(hwnd, message, wParam, lParam);
        }
        return result;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_window::create_window(int x_pos, int y_pos, int client_width, int client_height)
    {
        HINSTANCE instance = GetModuleHandleA(nullptr);

        static constexpr char const *class_name = "GL_CONTEXT_WINDOW_CLASS";
        static constexpr char const *window_title = "GL Window";

        // register window class

        WNDCLASSEXA wcex{};
        memset(&wcex, 0, sizeof(wcex));
        wcex.cbSize = sizeof(WNDCLASSEXA);
        wcex.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcex.lpfnWndProc = static_cast<WNDPROC>(wnd_proc_proxy);
        wcex.hInstance = instance;
        wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.lpszClassName = class_name;

        if(!RegisterClassExA(&wcex)) {
            return -1;
        }

        // create temp render context

        HWND temp_hwnd = CreateWindowExA(0, class_name, "", 0, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
        if(temp_hwnd == nullptr) {
            return -2;
        }

        HDC temp_dc = GetDC(temp_hwnd);
        if(temp_dc == nullptr) {
            return -3;
        }

        PIXELFORMATDESCRIPTOR temp_pixel_format_desc{};
        temp_pixel_format_desc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        temp_pixel_format_desc.nVersion = 1;
        temp_pixel_format_desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
        temp_pixel_format_desc.iPixelType = PFD_TYPE_RGBA;
        temp_pixel_format_desc.cColorBits = 32;
        temp_pixel_format_desc.cAlphaBits = 8;
        temp_pixel_format_desc.cDepthBits = 24;
        int temp_pixelFormat = ChoosePixelFormat(temp_dc, &temp_pixel_format_desc);
        if(temp_pixelFormat == 0) {
            return -4;
        }

        if(!SetPixelFormat(temp_dc, temp_pixelFormat, &temp_pixel_format_desc)) {
            return -5;
        }

        HGLRC temp_render_context = wglCreateContext(temp_dc);
        if(temp_render_context == nullptr) {
            return -6;
        }

        // activate temp render context so we can...

        wglMakeCurrent(temp_dc, temp_render_context);

        // ...get some opengl function pointers

        init_gl_functions();

        // now opengl functions are available, create actual window

        fullscreen = false;

        RECT rect{ x_pos, y_pos, client_width, client_height };
        DWORD style = WS_OVERLAPPEDWINDOW;
        if(!AdjustWindowRect(&rect, style, false)) {
            return -7;
        }

#if 0
        HMONITOR monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

        MONITORINFO mi = { sizeof(mi) };
        if(!GetMonitorInfoA(monitor, &mi)) {
            return -8;
        }

        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        int x = (mi.rcMonitor.right - mi.rcMonitor.left - width) / 2;
        int y = (mi.rcMonitor.bottom - mi.rcMonitor.top - height) / 2;
#else
        int x = rect.left;
        int y = rect.top;
        int width = rect.right - x;
        int height = rect.bottom - y;
#endif

        hwnd = CreateWindowExA(0, class_name, window_title, style, x, y, width, height, nullptr, nullptr, instance, this);
        if(hwnd == nullptr) {
            return -8;
        }

        window_dc = GetDC(hwnd);
        if(window_dc == nullptr) {
            return -9;
        }

        // create actual render context

        // clang-format off

        static constexpr int pixel_attributes[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_SWAP_METHOD_ARB, WGL_SWAP_EXCHANGE_ARB,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB, 32,
            WGL_ALPHA_BITS_ARB, 8,
            WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
            WGL_SAMPLES_ARB, 8,
            WGL_DEPTH_BITS_ARB, 24,
            0 };

        static constexpr int context_attributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 0,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0 };

        // clang-format on

        int pixel_format;
        UINT num_formats;
        BOOL status = wglChoosePixelFormatARB(window_dc, pixel_attributes, nullptr, 1, &pixel_format, &num_formats);
        if(!status || num_formats == 0) {
            return -10;
        }

        PIXELFORMATDESCRIPTOR pixel_format_desc{};
        DescribePixelFormat(window_dc, pixel_format, sizeof(PIXELFORMATDESCRIPTOR), &pixel_format_desc);

        if(!SetPixelFormat(window_dc, pixel_format, &pixel_format_desc)) {
            return -11;
        }

        render_context = wglCreateContextAttribsARB(window_dc, nullptr, context_attributes);
        if(render_context == nullptr) {
            return -12;
        }

        // destroy temp context and window

        wglMakeCurrent(temp_dc, nullptr);
        wglDeleteContext(temp_render_context);
        ReleaseDC(temp_hwnd, temp_dc);
        DestroyWindow(temp_hwnd);

        // activate the true render context

        wglMakeCurrent(window_dc, render_context);
        wglSwapIntervalEXT(1);

        // draw something before the window is shown so that we don't get a flash of some random color

        GL_CHECK(glViewport(0, 0, client_width, client_width));
        GL_CHECK(glClearColor(0, 0, 0, 1));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

        GL_CHECK(SwapBuffers(window_dc));

        // info some GL stuff

        LOG_INFO("GL Version: {}", (char const *)glGetString(GL_VERSION));
        LOG_INFO("GL Vendor: {}", (char const *)glGetString(GL_VENDOR));
        LOG_INFO("GL Renderer: {}", (char const *)glGetString(GL_RENDERER));
        LOG_INFO("GL Shader language version: {}", (char const *)glGetString(GL_SHADING_LANGUAGE_VERSION));

        // init ImGui

        // setup shaders

        if(solid_program.init() != 0) {
            return -13;
        }

        if(layer_program.init() != 0) {
            return -13;
        }

        if(color_program.init() != 0) {
            return -13;
        }

        if(textured_program.init() != 0) {
            return -13;
        }

        if(overlay.init(color_program) != 0) {
            return -13;
        }

        fullscreen_blit_verts.init(textured_program, 3);

        // load ini file (and maybe kick off a bunch of file loading threads)

        glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA, GL_SAMPLES, 1, &max_multisamples);

        LOG_INFO("MAX GL Multisamples: {}", max_multisamples);

        if(multisample_count > max_multisamples) {
            multisample_count = max_multisamples;
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::ui(int wide, int high)
    {
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::draw(int width_pixels, int height_pixels)
    {
        window_width = width_pixels;
        window_height = height_pixels;

        if(overlay.vertex_array.vbo_id == 0) {
            return;
        }

        if(window_width == 0 || window_height == 0) {
            return;
        }

        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

        ui(width_pixels, height_pixels);

        if(!IsWindowVisible(hwnd)) {
            return;
        }

        render();

        SwapBuffers(window_dc);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::render()
    {
        GL_CHECK(glViewport(0, 0, window_width, window_height));

        gl_color::float4 back_col(background_color);
        GL_CHECK(glClearColor(back_col[0], back_col[1], back_col[2], back_col[3]));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

        // setup gl viewport and transform matrix

        if(my_target.width != window_width || my_target.height != window_height || my_target.num_samples != multisample_count) {
            my_target.cleanup();
            my_target.init(window_width, window_height, multisample_count, 1);
        }

        // draw the gerber layers

        update_view_rect();

        gl_matrix projection_matrix_invert_y;
        gl_matrix projection_matrix;
        gl_matrix view_matrix;
        gl_matrix world_transform_matrix;
        gl_matrix screen_matrix;

        // make a 1:1 screen matrix with origin in top left

        make_ortho(projection_matrix_invert_y, window_width, -window_height);
        make_translate(view_matrix, 0, static_cast<float>(-window_size.y));
        matrix_multiply(projection_matrix_invert_y, view_matrix, screen_matrix);

        // make world to window matrix with origin in bottom left

        make_ortho(projection_matrix, window_width, window_height);
        make_world_to_window_transform(view_matrix, window_rect, view_rect);
        matrix_multiply(projection_matrix, view_matrix, world_transform_matrix);

        // setup full screen copy vertices for window size

        gl_vertex_textured quad[3] = { { 0, 0, 0, 0 }, { static_cast<float>(window_size.x) * 2, 0, 2, 0 }, { 0, static_cast<float>(window_size.y) * 2, 0, 2 } };
        fullscreen_blit_verts.activate();
        gl_vertex_textured *v;
        GL_CHECK(v = (gl_vertex_textured *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
        if(v != nullptr) {
            memcpy(v, quad, 3 * sizeof(gl_vertex_textured));
        } else {
            LOG_ERROR("Huh? glMapBuffer returned NULL!?");
            return;
        }
        GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));

        glEnable(GL_BLEND);

        glLineWidth(outline_thickness);
        // draw layers in reverse so top layer (which is first in the list) is drawn last

        // for(size_t n = layers.size(); n != 0;) {
        //
        //     gerber_layer *layer = layers[--n];
        //
        //     if(!layer->hide) {
        //
        //         // draw the gerber layer into the render texture
        //
        //         layer_program.use();
        //         my_target.bind_framebuffer();
        //         GL_CHECK(glUniformMatrix4fv(layer_program.transform_location, 1, true, world_transform_matrix));
        //
        //         GL_CHECK(glClearColor(0, 0, 0, 0));
        //         GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
        //         layer->draw(wireframe, outline_thickness);
        //
        //         // draw the render to the window
        //
        //         glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        //
        //         GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        //
        //         textured_program.use();
        //
        //         my_target.bind_textures();
        //
        //         gl_color::float4 rc(layer->fill_color);
        //         gl_color::float4 gc(layer->clear_color);
        //         gl_color::float4 bc(layer->outline_color);
        //         glUniform4fv(textured_program.red_color_uniform, 1, (GLfloat *)&rc);
        //         glUniform4fv(textured_program.green_color_uniform, 1, (GLfloat *)&gc);
        //         glUniform4fv(textured_program.blue_color_uniform, 1, (GLfloat *)&bc);
        //         glUniform1f(textured_program.alpha_uniform, layer->alpha / 255.0f);
        //         glUniform1i(textured_program.num_samples_uniform, my_target.num_samples);
        //         glUniform1i(textured_program.cover_sampler, 0);
        //         glUniformMatrix4fv(textured_program.transform_location, 1, true, projection_matrix);
        //
        //         fullscreen_blit_verts.activate();
        //
        //         glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        //         glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //         glDrawArrays(GL_TRIANGLES, 0, 3);
        //     }
        // }
        //
        // // now let's draw a (some?) selected entites directly onto the screen
        // solid_program.use();
        // GL_CHECK(glUniformMatrix4fv(solid_program.transform_location, 1, true, world_transform_matrix));
        //
        // solid_program.set_color(0xffffffff);
        //
        // for(auto const &l : layers) {
        //     l->layer->fill_entities(l->selected_entities);
        // }
        // create overlay drawlist (axes, extents etc)

        glLineWidth(1);
        overlay.reset();
        if(mouse_mode == mouse_drag_zoom_select) {
            rect drag_rect_corrected = correct_aspect_ratio(window_rect.aspect_ratio(), drag_rect, aspect_expand);
            overlay.add_rect(drag_rect_corrected, 0x80ffff00);
            overlay.add_rect(drag_rect, 0x800000ff);
            overlay.add_outline_rect(drag_rect, 0xffffffff);
        }

        vec2d origin = window_pos_from_world_pos({ 0, 0 });

        if(show_axes) {    // show_axes
            overlay.lines();
            overlay.add_line({ 0, origin.y }, { window_size.x, origin.y }, axes_color);
            overlay.add_line({ origin.x, 0 }, { origin.x, window_size.y }, axes_color);
        }

        // if(show_extent && selected_layer != nullptr) {
        //     rect const &extent = selected_layer->layer->gerber_file->image.info.extent;
        //     rect s{ window_pos_from_world_pos(extent.min_pos), window_pos_from_world_pos(extent.max_pos) };
        //     overlay.add_outline_rect(s, extent_color);
        // }
        //
        if(mouse_mode == mouse_drag_select) {
            rect f{ drag_mouse_start_pos, drag_mouse_cur_pos };
            uint32_t color = 0x60ff8020;
            if(f.min_pos.x > f.max_pos.x) {
                color = 0x6080ff20;
            }
            overlay.add_rect(f, color);
            overlay.add_outline_rect(f, 0xffffffff);
        }

        // draw overlay

        color_program.use();

        GL_CHECK(glUniformMatrix4fv(color_program.transform_location, 1, true, screen_matrix));

        overlay.draw();
    }

}    // namespace gerber_3d
