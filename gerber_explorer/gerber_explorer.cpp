#include <filesystem>

#include "imgui.h"
#include "imgui_internal.h"

#include <nfd.h>

#include "gerber_explorer.h"
#include "gl_matrix.h"
#include "gl_colors.h"

LOG_CONTEXT("gerber_explorer", debug);

namespace
{
    using gerber_lib::gerber_2d::rect;
    using gerber_lib::gerber_2d::vec2d;
    using namespace gerber_3d;

    long long const zoom_lerp_time_ms = 700;

    double const drag_select_offset_start_distance = 16;

    uint32_t layer_colors[] = { gl_color::dark_green,    gl_color::dark_cyan,        gl_color::green, gl_color::lime_green,
                                gl_color::antique_white, gl_color::corn_flower_blue, gl_color::gold };

    //////////////////////////////////////////////////////////////////////
    // make a rectangle have a certain aspect ratio by shrinking or expanding it

    enum aspect_ratio_correction
    {
        aspect_shrink,
        aspect_expand,
    };

    //////////////////////////////////////////////////////////////////////

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

}    // namespace

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::world_pos_from_window_pos(vec2d const &p) const
{
    vec2d scale = view_rect.size().divide(window_size);
    return vec2d{ p.x, window_size.y - p.y }.multiply(scale).add(view_rect.min_pos);
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::window_pos_from_world_pos(vec2d const &p) const
{
    vec2d scale = window_size.divide(view_rect.size());
    vec2d pos = p.subtract(view_rect.min_pos).multiply(scale);
    return { pos.x, window_size.y - pos.y };
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::fit_to_window()
{
    if(selected_layer != nullptr && selected_layer->is_valid()) {
        zoom_to_rect(selected_layer->extent());
    } else {
        rect all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
        for(auto layer : layers) {
            all = all.union_with(layer->extent());
        }
        zoom_to_rect(all);
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::zoom_to_rect(rect const &zoom_rect, double border_ratio)
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

void gerber_explorer::zoom_image(vec2d const &pos, double zoom_scale)
{
    // normalized position within view_rect
    vec2d zoom_pos = vec2d{ (double)pos.x, window_size.y - (double)pos.y }.divide(window_size);

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

void gerber_explorer::update_view_rect()
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
        double t = (double)remaining_ms / (double)zoom_lerp_time_ms;
        double d = lerp(t);
        vec2d dmin = target_view_rect.min_pos.subtract(source_view_rect.min_pos).scale(d);
        vec2d dmax = target_view_rect.max_pos.subtract(source_view_rect.max_pos).scale(d);
        view_rect = { source_view_rect.min_pos.add(dmin), source_view_rect.max_pos.add(dmax) };
        vec2d wv = window_pos_from_world_pos(view_rect.min_pos);
        vec2d tv = window_pos_from_world_pos(target_view_rect.min_pos);
        if(wv.subtract(tv).length() <= 1) {
            view_rect = target_view_rect;
            zoom_anim = false;
        }
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_key(int key, int scancode, int action, int mods)
{
    // auto layer = layers[0];
    // auto calls = layer->layer->draw_calls;
    // switch(action) {
    // case GLFW_PRESS:
    // case GLFW_REPEAT:
    //     switch(key) {
    //     case GLFW_KEY_UP: {
    //         debug_draw_call += 1;
    //         if(debug_draw_call >= calls.size()) {
    //             debug_draw_call = calls.size() - 1;
    //         }
    //         LOG_INFO("draw_call {}", debug_draw_call);
    //     } break;
    //     case GLFW_KEY_DOWN: {
    //         debug_draw_call -= 1;
    //         if(debug_draw_call < 0) {
    //             debug_draw_call = 0;
    //         }
    //         LOG_INFO("draw_call {}", debug_draw_call);
    //     } break;
    //     case GLFW_KEY_LEFT: {
    //         debug_outline_line -= 1;
    //         auto const &call = calls[debug_draw_call];
    //         auto const &verts = layer->layer->outline_vertices;
    //         if(debug_outline_line < 0) {
    //             debug_outline_line = 0;
    //         }
    //         LOG_INFO("line {}", debug_outline_line);
    //     } break;
    //     case GLFW_KEY_RIGHT: {
    //         debug_outline_line += 1;
    //         auto const &call = calls[debug_draw_call];
    //         auto const &verts = layer->layer->outline_vertices;
    //         if(debug_outline_line >= call.outline_length) {
    //             debug_outline_line = call.outline_length - 1;
    //         }
    //         LOG_INFO("line {}", debug_outline_line);
    //     } break;
    //     }
    //     break;
    // case GLFW_RELEASE:
    //     break;
    // }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_scroll(double xoffset, double yoffset)
{
    double scale_factor = (yoffset > 0) ? 1.1 : 0.9;
    zoom_image(mouse_pos, scale_factor);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_button(int button, int action, int mods)
{
    switch(action) {
    case GLFW_PRESS:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if((mods & GLFW_MOD_CONTROL) != 0) {
                set_mouse_mode(mouse_drag_zoom_select, mouse_pos);
            } else {
                set_mouse_mode(mouse_drag_maybe_select, mouse_pos);
            }
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_pan, mouse_pos);
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_zoom, mouse_pos);
            break;
        }
        break;
    case GLFW_RELEASE:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
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
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_none, {});
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_none, {});
            break;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_move(double xpos, double ypos)
{
    mouse_pos = { xpos, ypos };

    switch(mouse_mode) {

    case mouse_drag_pan: {
        vec2d new_mouse_pos = world_pos_from_window_pos(mouse_pos);
        vec2d old_mouse_pos = world_pos_from_window_pos(drag_mouse_start_pos);
        view_rect = view_rect.offset(new_mouse_pos.subtract(old_mouse_pos).negate());
        drag_mouse_start_pos = mouse_pos;
    } break;

    case mouse_drag_zoom: {
        vec2d d = mouse_pos.subtract(drag_mouse_cur_pos);
        double factor = (d.x - d.y) * 0.01;
        // sometimes cursor coords jump for reasons I don't understand so clamp
        factor = std::max(-0.15, std::min(factor, 0.15));
        zoom_image(drag_mouse_start_pos, 1.0 + factor);
        drag_mouse_cur_pos = mouse_pos;
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

void gerber_explorer::on_closed()
{
    NFD_Quit();
    gerber_load_thread.request_stop();
    loader_semaphore.release();

    window_state = get_window_state();
    settings.window_width = window_state.width;
    settings.window_height = window_state.height;
    settings.window_xpos = window_state.x;
    settings.window_ypos = window_state.y;
    settings.window_maximized = window_state.isMaximized;

    settings.files.clear();
    for(auto layer : layers) {
        settings.files.push_back(layer->layer->gerber_file->filename);
    }
    settings.save();
    gl_window::on_closed();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::set_mouse_mode(mouse_drag_action action, vec2d const &pos)
{
    switch(action) {

    case mouse_drag_none:
        zoom_anim = mouse_mode == mouse_drag_zoom_select;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;

    case mouse_drag_pan:
        drag_mouse_start_pos = pos;
        zoom_anim = false;
        break;

    case mouse_drag_zoom:
        zoom_anim = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        drag_mouse_cur_pos = pos;
        drag_mouse_start_pos = pos;
        break;

    case mouse_drag_zoom_select:
        zoom_anim = false;
        drag_mouse_start_pos = pos;
        drag_rect = {};
        break;

    case mouse_drag_maybe_select: {
        zoom_anim = false;
        drag_mouse_start_pos = pos;
        // vec2d world_pos = world_pos_from_window_pos(pos);
        // for(auto const &l : layers) {
        //     l->layer->tesselator.pick_entities(world_pos, l->selected_entities);
        // }
    } break;

    case mouse_drag_select:
        zoom_anim = false;
        drag_mouse_cur_pos = pos;
        drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
        break;
    }
    mouse_mode = action;
}

//////////////////////////////////////////////////////////////////////

bool gerber_explorer::on_init()
{
    NFD_Init();

    glfwGetWindowSize(window, &window_width, &window_height);
    window_size.x = window_width;
    window_size.y = window_height;
    window_rect = { { 0, 0 }, window_size };
    view_rect = window_rect.offset(window_size.scale(-0.5));

    solid_program.init();
    color_program.init();
    layer_program.init();
    textured_program.init();

    overlay.init(color_program);

    fullscreen_blit_verts.init(textured_program, 3);

    glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA, GL_SAMPLES, 1, &max_multisamples);

    LOG_INFO("MAX GL Multisamples: {}", max_multisamples);

    if(multisample_count > max_multisamples) {
        multisample_count = max_multisamples;
    }

    gerber_load_thread = std::jthread([this](std::stop_token const &st) { load_gerbers(st); });

    settings.load();

    window_state.width = settings.window_width;
    window_state.height = settings.window_height;
    window_state.x = settings.window_xpos;
    window_state.y = settings.window_ypos;
    window_state.isMaximized = settings.window_maximized;

    for(auto const &filename : settings.files) {
        load_gerber(filename.c_str());
    }

    return true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_gerbers(std::stop_token const &st)
{
    LOG_CONTEXT("loader", debug);
    LOG_DEBUG("Waiting for filenames...");
    while(!st.stop_requested()) {
        loader_semaphore.acquire();
        if(!st.stop_requested()) {
            std::lock_guard loader_lock(loader_mutex);
            if(gerber_filenames_to_load.empty()) {
                LOG_WARNING("Huh? Where's the filename?");
                continue;
            }
            std::string filename = gerber_filenames_to_load.front();
            gerber_filenames_to_load.pop_front();
            LOG_DEBUG("Loading {}", filename);
            gerber_lib::gerber *g = new gerber_lib::gerber();
            gerber_lib::gerber_error_code err = g->parse_file(filename.c_str());
            if(err == gerber_lib::ok) {
                gerber_layer *layer = new gerber_layer();
                layer->layer = new gl_drawer();
                layer->layer->set_gerber(g);
                layer->layer->program = &layer_program;
                layer->fill_color = gl_color::alice_blue;
                layer->clear_color = gl_color::clear;
                layer->outline_color = gl_color::magenta;
                layer->outline = false;
                layer->name = std::filesystem::path(g->filename).filename().string();
                LOG_DEBUG("Finished loading {}", filename);
                {
                    std::lock_guard loaded_lock(loaded_mutex);
                    loaded_layers.push_back(layer);
                }
            } else {
                LOG_ERROR("Error loading {} ({})", filename, gerber_lib::get_error_text(err));
            }
        }
    }
    LOG_DEBUG("Loader is exiting...");
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_gerber(char const *filename)
{
    {
        std::lock_guard lock(loader_mutex);
        gerber_filenames_to_load.push_back(filename);
    }
    loader_semaphore.release();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_render()
{
    // call on_finished_loading (which creates gl buffers) in main thread
    {
        std::lock_guard loaded_lock(loaded_mutex);
        if(!loaded_layers.empty()) {
            gerber_layer *loaded_layer = loaded_layers.front();
            loaded_layers.pop_front();
            loaded_layer->layer->on_finished_loading();
            loaded_layer->fill_color = layer_colors[layers.size() % gerber_util::array_length(layer_colors)];
            layers.push_back(loaded_layer);
        }
    }

    int framebuffer_width;
    int framebuffer_height;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    // ImGuiDockNode *central_node = ImGui::DockBuilderGetCentralNode(dockspace_id);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Open", nullptr, nullptr)) {
                const nfdpathset_t *paths;
                nfdopendialogu8args_t args{};
                nfdresult_t result = NFD_OpenDialogMultipleU8_With(&paths, &args);
                switch(result) {
                case NFD_OKAY: {
                    nfdpathsetsize_t path_count;
                    if(NFD_PathSet_GetCount(paths, &path_count) == NFD_OKAY) {
                        for(size_t i = 0; i < path_count; ++i) {
                            nfdu8char_t *outPath;
                            if(NFD_PathSet_GetPath(paths, i, &outPath) == NFD_OKAY) {
                                load_gerber(outPath);
                                NFD_FreePathU8(outPath);
                            }
                        }
                    }
                    NFD_PathSet_Free(paths);
                } break;
                case NFD_CANCEL:
                    LOG_DEBUG("Cancelled");
                    break;
                default:
                    LOG_ERROR("Error: {}", NFD_GetError());
                    break;
                }
            }
            // ImGui::MenuItem("Stats", nullptr, &show_stats);
            // ImGui::MenuItem("Options", nullptr, &show_options);
            if(ImGui::MenuItem("Close all", nullptr, nullptr)) {
                while(!layers.empty()) {
                    auto l = layers.front();
                    layers.pop_front();
                    delete l;
                }
                selected_layer = nullptr;
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Exit", "Esc", nullptr)) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View")) {
            if(ImGui::MenuItem("Fit to window", nullptr, nullptr, !layers.empty())) {
                fit_to_window();
            }
            ImGui::MenuItem("Wireframe", nullptr, &settings.wireframe);
            ImGui::MenuItem("Show Axes", nullptr, &settings.show_axes);
            ImGui::MenuItem("Show Extent", nullptr, &settings.show_extent);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();

    ImGui::Begin("gerbx");
    {
        int num_controls = 2;
        float controls_width = ImGui::GetFrameHeight() * num_controls + ImGui::GetStyle().ItemSpacing.x * num_controls;
        gerber_layer *move_from = nullptr;
        gerber_layer *move_to = nullptr;
        static gerber_layer *current_selected_ptr = nullptr;
        if(ImGui::BeginTable("Layers", 2, ImGuiTableFlags_SizingFixedFit, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, controls_width);
            int n = 0;
            for(auto it = layers.begin(); it != layers.end(); ++it) {
                gerber_layer *l = *it;
                ImGui::PushID(l);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                bool is_selected = (current_selected_ptr == l);
                float row_height = ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.y;
                if(ImGui::Selectable(
                       l->name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, row_height))) {
                    current_selected_ptr = l;
                    selected_layer = l;
                }
                ImGui::AlignTextToFramePadding();
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("REORDER_LAYER", &l, sizeof(gerber_layer *));
                    ImGui::Text("Moving %s", l->name.c_str());    // Preview text while dragging
                    ImGui::EndDragDropSource();
                }

                if(ImGui::BeginDragDropTarget()) {
                    if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("REORDER_LAYER")) {
                        move_from = *(gerber_layer **)payload->Data;
                        move_to = l;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::Checkbox("##hide", &l->show);
                ImGui::SameLine();
                ImGui::ColorEdit4("##color",
                                  l->fill_color.f,
                                  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB |
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Uint8 |
                                      ImGuiColorEditFlags_PickerHueWheel);
                ImGui::PopID();
                n += 1;
            }
        }
        if(move_from != nullptr && move_to != nullptr) {
            auto it_from = std::find(layers.begin(), layers.end(), move_from);
            auto it_to = std::find(layers.begin(), layers.end(), move_to);

            // Move move_from to the position of move_to
            layers.erase(it_from);
            layers.insert(it_to, move_from);
        }
        ImGui::EndTable();
    }
    ImGui::End();

    ImVec2 pos{ 0, 0 };

    // if(central_node) {
    //     ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    //     pos = ImVec2(central_node->Pos.x - main_viewport->Pos.x, central_node->Pos.y - main_viewport->Pos.y);
    //     ImVec2 size = central_node->Size;
    //     window_width = (int)size.x;
    //     window_height = (int)size.y;
    // }
    glfwGetWindowSize(window, &window_width, &window_height);
    vec2d new_window_size;
    new_window_size.x = window_width;
    new_window_size.y = window_height;
    if(new_window_size.x != window_size.x || new_window_size.y != window_size.y) {
        vec2d scale_factor = new_window_size.divide(window_size);
        window_size = new_window_size;
        window_rect = { { 0, 0 }, window_size };
        vec2d new_view_size = view_rect.size().multiply(scale_factor);
        view_rect.max_pos = view_rect.min_pos.add(new_view_size);
        LOG_INFO("SIZE {}, POS {},{}", window_size, pos.x, pos.y);
    }

    update_view_rect();

    gl_matrix flip_y_matrix = make_ortho(window_width, -window_height);
    gl_matrix offset_y_matrix = make_translate(0, (float)-window_height);
    gl_matrix scale_matrix = make_scale((float)(window_rect.width() / view_rect.width()), (float)(window_rect.height() / view_rect.height()));
    gl_matrix origin_matrix = make_translate(-(float)view_rect.min_pos.x, -(float)view_rect.min_pos.y);
    gl_matrix view_matrix = matrix_multiply(scale_matrix, origin_matrix);
    screen_matrix = matrix_multiply(flip_y_matrix, offset_y_matrix);
    projection_matrix = make_ortho(window_width, window_height);
    world_matrix = matrix_multiply(projection_matrix, view_matrix);

    GL_CHECK(glClearColor(0.1f, 0.2f, 0.3f, 1.0f));
    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glDisable(GL_CULL_FACE));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    std::array<gl_vertex_textured, 3> fullscreen_triangle;
    fullscreen_triangle[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
    fullscreen_triangle[1] = { window_width * 2.0f, 0.0f, 2.0f, 0.0f };
    fullscreen_triangle[2] = { 0, window_height * 2.0f, 0.0f, 2.0f };
    fullscreen_blit_verts.activate();
    update_buffer<GL_ARRAY_BUFFER>(fullscreen_triangle);
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    if(my_target.width != window_width || my_target.height != window_height || my_target.num_samples != multisample_count) {
        my_target.cleanup();
        my_target.init(window_width, window_height, multisample_count, 1);
    }

    for(auto r = layers.rbegin(); r != layers.crend(); ++r) {
        gerber_layer &layer = **r;

        if(layer.show) {
            layer_program.use();
            my_target.bind_framebuffer();
            GL_CHECK(glUniformMatrix4fv(layer_program.transform_location, 1, true, world_matrix.m));

            GL_CHECK(glViewport(0, 0, window_size.x, window_size.y));
            GL_CHECK(glClearColor(0, 0, 0, 0));
            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
            layer.draw(settings.wireframe, 1.0f);

            // draw the render to the window

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            GL_CHECK(glViewport(0, 0, window_size.x, window_size.y));
            // GL_CHECK(glViewport(pos.x, pos.y, window_size.x, window_size.y));

            GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

            textured_program.use();

            my_target.bind_textures();

            glUniform4fv(textured_program.red_color_uniform, 1, layer.fill_color.f);
            glUniform4fv(textured_program.green_color_uniform, 1, layer.clear_color.f);
            glUniform4fv(textured_program.blue_color_uniform, 1, layer.outline_color.f);
            glUniform1f(textured_program.alpha_uniform, layer.alpha / 255.0f);
            glUniform1i(textured_program.num_samples_uniform, my_target.num_samples);
            glUniform1i(textured_program.cover_sampler, 0);
            glUniformMatrix4fv(textured_program.transform_location, 1, true, projection_matrix.m);

            fullscreen_blit_verts.activate();

            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }
    glLineWidth(1.0f);

    overlay.reset();

    if(mouse_mode == mouse_drag_zoom_select) {
        rect drag_rect_corrected = correct_aspect_ratio(window_rect.aspect_ratio(), drag_rect, aspect_expand);
        overlay.add_rect(drag_rect_corrected, 0x80ffff00);
        overlay.add_rect(drag_rect, 0x800000ff);
        overlay.add_outline_rect(drag_rect, 0xffffffff);
    }

    vec2d origin = window_pos_from_world_pos({ 0, 0 });

    uint32_t axes_color = gl_color::cyan;
    uint32_t extent_color = gl_color::yellow;

    if(settings.show_axes) {    // show_axes
        overlay.lines();
        overlay.add_line({ 0, origin.y }, { window_size.x, origin.y }, axes_color);
        overlay.add_line({ origin.x, 0 }, { origin.x, window_size.y }, axes_color);
    }

    if(settings.show_extent && selected_layer != nullptr && selected_layer->is_valid()) {
        rect extent = selected_layer->extent();
        if(extent.width() != 0 && extent.height() != 0) {
            rect s{ window_pos_from_world_pos(extent.min_pos), window_pos_from_world_pos(extent.max_pos) };
            overlay.add_outline_rect(s, extent_color);
        }
    }

    if(mouse_mode == mouse_drag_select) {
        rect f{ drag_mouse_start_pos, drag_mouse_cur_pos };
        uint32_t color = 0x60ff8020;
        if(f.min_pos.x > f.max_pos.x) {
            color = 0x6080ff20;
        }
        overlay.add_rect(f, color);
        overlay.add_outline_rect(f, 0xffffffff);
    }

    color_program.use();

    GL_CHECK(glUniformMatrix4fv(color_program.transform_location, 1, true, screen_matrix.m));

    overlay.draw();
}
