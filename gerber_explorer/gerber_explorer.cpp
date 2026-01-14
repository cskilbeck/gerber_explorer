#include <filesystem>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include <nfd.h>

#include "gerber_explorer.h"
#include "gl_matrix.h"
#include "gl_colors.h"
#include "util.h"

#include "assets/matsym_codepoints_utf8.h"

LOG_CONTEXT("gerber_explorer", debug);

static double start_angle = 0;
static double end_angle = 190;

namespace
{
    using gerber_lib::rect;
    using gerber_lib::vec2d;
    using gerber_lib::vec2f;
    using namespace gerber_3d;

    rect arc_extent;

    long long const zoom_lerp_time_ms = 700;

    double const drag_select_offset_start_distance = 16;

    gl::color layer_colors[] = { (gl::color)gl::colors::dark_green, (gl::color)gl::colors::dark_cyan,     (gl::color)gl::colors::green,
                                 (gl::color)gl::colors::lime_green, (gl::color)gl::colors::antique_white, (gl::color)gl::colors::corn_flower_blue,
                                 (gl::color)gl::colors::gold };

    size_t next_layer_color{ 0 };

    int next_index{ 1 };

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

std::string gerber_explorer::window_name() const
{
    return app_friendly_name;
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::world_pos_from_window_pos(vec2d const &p) const
{
    return vec2d{ p.x, p.y }.divide(view_scale).add(view_rect.min_pos);
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::world_rect_from_window_rect(rect const &r) const
{
    vec2d min = world_pos_from_window_pos(r.min_pos);
    vec2d max = world_pos_from_window_pos(r.max_pos);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::board_pos_from_window_pos(vec2d const &p) const
{
    vec2d pos = world_pos_from_window_pos(p);
    return pos.subtract(board_center).multiply(flip_xy).add(board_center);
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::board_rect_from_window_rect(rect const &r) const
{
    vec2d min = world_pos_from_window_pos(r.min_pos);
    vec2d max = world_pos_from_window_pos(r.max_pos);
    min = min.subtract(board_center).multiply(flip_xy).add(board_center);
    max = max.subtract(board_center).multiply(flip_xy).add(board_center);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

vec2d gerber_explorer::window_pos_from_world_pos(vec2d const &p) const
{
    return p.subtract(view_rect.min_pos).multiply(view_scale);
}

//////////////////////////////////////////////////////////////////////

rect gerber_explorer::window_rect_from_world_rect(rect const &r) const
{
    vec2d min = window_pos_from_world_pos(r.min_pos);
    vec2d max = window_pos_from_world_pos(r.max_pos);
    return rect(min, max).normalize();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::fit_to_window()
{
    if(selected_layer != nullptr && selected_layer->is_valid()) {
        zoom_to_rect(selected_layer->extent());
    } else {
        should_fit_to_window = true;
        update_board_extent();
        zoom_to_rect(board_extent);
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::zoom_to_rect(rect const &zoom_rect, double border_ratio)
{
    if(viewport_width == 0 || viewport_height == 0) {
        return;
    }
    double aspect_ratio = (double)viewport_width / viewport_height;
    rect new_rect = correct_aspect_ratio(aspect_ratio, zoom_rect, aspect_expand);
    vec2d mid = new_rect.mid_point();
    vec2d siz = new_rect.size().scale(border_ratio / 2);
    target_view_rect = { mid.subtract(siz), mid.add(siz) };
    source_view_rect = view_rect;
    target_view_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(zoom_lerp_time_ms);
    zoom_anim = true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::zoom_at_point(vec2d const &zoom_pos, double zoom_scale)
{
    vec2d bl = view_rect.min_pos.subtract(zoom_pos).multiply(zoom_scale).add(zoom_pos);
    vec2d tr = view_rect.max_pos.subtract(zoom_pos).multiply(zoom_scale).add(zoom_pos);
    view_rect = { bl, tr };
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

void gerber_explorer::on_drop(int count, const char **paths)
{
    for(int i = 0; i < count; ++i) {
        load_gerber(settings::layer_t{ paths[i] });
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_key(int key, int scancode, int action, int mods)
{
    if((action == GLFW_PRESS || action == GLFW_REPEAT)) {
        if(mods == 0) {
            switch(key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_F:
                fit_to_window();
                break;
            case GLFW_KEY_X:
                settings.flip_x = !settings.flip_x;
                break;
            case GLFW_KEY_Y:
                settings.flip_y = !settings.flip_y;
                break;
            case GLFW_KEY_W:
                settings.wireframe = !settings.wireframe;
                break;
            case GLFW_KEY_A:
                settings.show_axes = !settings.show_axes;
                break;
            case GLFW_KEY_E:
                settings.show_extent = !settings.show_extent;
                break;
            }
        } else if(mods & GLFW_MOD_CONTROL) {
            switch(key) {
            case GLFW_KEY_O:
                file_open();
                break;
            case GLFW_KEY_S: {
                auto save_path = save_file_dialog();
                if(save_path.has_value()) {
                    save_settings(save_path.value());
                }
            } break;
            case GLFW_KEY_L: {
                auto load_path = load_file_dialog();
                if(load_path.has_value()) {
                    load_settings(load_path.value());
                }
            } break;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_scroll(double xoffset, double yoffset)
{
    double scale_factor = (yoffset > 0) ? 0.9 : 1.1;
    zoom_at_point(world_pos_from_window_pos(mouse_pos), scale_factor);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_button(int button, int action, int mods)
{
    switch(action) {
    case GLFW_PRESS:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if((mods & GLFW_MOD_CONTROL) != 0) {
                set_mouse_mode(mouse_drag_zoom_select);
            } else {
                set_mouse_mode(mouse_drag_maybe_select);
            }
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_pan);
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_zoom);
            break;
        }
        break;
    case GLFW_RELEASE:
        switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if(mouse_mode == mouse_drag_zoom_select) {
                rect drag_rect_corrected = correct_aspect_ratio(viewport_rect.aspect_ratio(), drag_rect, aspect_expand);
                vec2d mn = drag_rect_corrected.min_pos;
                vec2d mx = drag_rect_corrected.max_pos;
                rect d = rect{ mn, mx }.normalize();
                if(d.width() > 2 && d.height() > 2) {
                    zoom_to_rect(world_rect_from_window_rect({ mn, mx }));
                    should_fit_to_window = false;
                }
            } else if(mouse_mode == mouse_drag_select && selected_layer != nullptr) {
                selected_layer->layer.select_hovered_entities();
            }
            set_mouse_mode(mouse_drag_none);
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            set_mouse_mode(mouse_drag_none);
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            set_mouse_mode(mouse_drag_none);
            break;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_mouse_move(double xpos, double ypos)
{
    mouse_pos = { xpos - viewport_xpos, viewport_height - (ypos - viewport_ypos) };
    mouse_did_move = true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::handle_mouse()
{
    if(mouse_did_move) {

        mouse_did_move = false;

        switch(mouse_mode) {

        case mouse_drag_pan: {
            vec2d new_mouse_pos = world_pos_from_window_pos(mouse_pos);
            vec2d old_mouse_pos = world_pos_from_window_pos(drag_mouse_start_pos);
            vec2d diff = new_mouse_pos.subtract(old_mouse_pos).negate();
            view_rect = view_rect.offset(diff);
            drag_mouse_start_pos = mouse_pos;
            should_fit_to_window = false;
        } break;

        case mouse_drag_zoom: {
            if(ignore_mouse_moves <= 0) {
                vec2d d = mouse_pos.subtract(drag_mouse_cur_pos);
                d.y *= -1;
                double factor = (d.x - d.y) * 0.01;
                factor = std::max(-0.25, std::min(factor, 0.25));
                zoom_at_point(mouse_world_pos, 1.0 - factor);
                should_fit_to_window = false;
            } else {
                ignore_mouse_moves -= 1;
            }
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
                set_mouse_mode(mouse_drag_select);
                mouse_world_pos = world_pos_from_window_pos(mouse_pos);
            }
        } break;

        case mouse_drag_select: {
            drag_mouse_cur_pos = mouse_pos;
            drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
            if(selected_layer != nullptr) {
                if(drag_rect.min_pos.x > drag_rect.max_pos.x) {
                    selected_layer->layer.flag_touching_entities(
                        board_rect_from_window_rect(drag_rect), entity_flags_t::hovered | entity_flags_t::selected, entity_flags_t::hovered);
                } else {
                    selected_layer->layer.flag_enclosed_entities(
                        board_rect_from_window_rect(drag_rect), entity_flags_t::hovered | entity_flags_t::selected, entity_flags_t::hovered);
                }
            }
        } break;

        default:
        case mouse_drag_none: {
            // Just hovering, highlight entities under the mouse if selected_layer != nullptr
            if(selected_layer != nullptr) {
                vec2d pos = board_pos_from_window_pos(mouse_pos);
                selected_layer->layer.flag_entities_at_point(pos, entity_flags_t::hovered, entity_flags_t::hovered);
            }
        } break;
        }
    }
}


//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_closed()
{
    NFD_Quit();
    gerber_load_thread.request_stop();
    loader_semaphore.release();
    save_settings(config_path(app_name, settings_filename));
    gl_window::on_closed();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_settings(std::filesystem::path const &path)
{
    settings.load(path);
    LOG_DEBUG("Settings loaded...");
    window_state.width = settings.window_width;
    window_state.height = settings.window_height;
    window_state.x = settings.window_xpos;
    window_state.y = settings.window_ypos;
    window_state.isMaximized = settings.window_maximized;
    for(auto const &layer : settings.files) {
        load_gerber(layer);
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_window_size(int w, int h)
{
    gl_window::on_window_size(w, h);
    window_width = w;
    window_height = h;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::on_window_refresh()
{
    gl_window::on_window_refresh();
    on_frame();
    if(should_fit_to_window) {
        view_rect = target_view_rect;
    }
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::save_settings(std::filesystem::path const &path)
{
    window_state = get_window_state();
    settings.window_width = window_state.width;
    settings.window_height = window_state.height;
    settings.window_xpos = window_state.x;
    settings.window_ypos = window_state.y;
    settings.window_maximized = window_state.isMaximized;

    settings.files.clear();
    int index = 1;
    for(auto it = layers.crbegin(); it != layers.crend(); ++it) {
        gerber_layer *layer = *it;
        if(layer->is_valid()) {
            settings.files.emplace_back(layer->filename(), layer->fill_color.to_string(), layer->visible, layer->invert, layer->draw_mode, index);
            index += 1;
        }
    }
    settings.save(path);
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::set_mouse_mode(mouse_drag_action action)
{
    switch(action) {

    case mouse_drag_none:
        zoom_anim = mouse_mode == mouse_drag_zoom_select;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;

    case mouse_drag_pan:
        drag_mouse_start_pos = mouse_pos;
        zoom_anim = false;
        break;

    case mouse_drag_zoom:
        zoom_anim = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mouse_world_pos = world_pos_from_window_pos(mouse_pos);
        drag_mouse_cur_pos = mouse_pos;
        drag_mouse_start_pos = mouse_pos;
        // ignore the next 2 mouse moves because sometimes they are kind of
        // random, something to do with GLFW_CURSOR_DISABLED/NORMAL
        ignore_mouse_moves = 2;
        break;

    case mouse_drag_zoom_select:
        zoom_anim = false;
        drag_mouse_start_pos = mouse_pos;
        drag_rect = {};
        break;

    case mouse_drag_maybe_select: {
        zoom_anim = false;
        drag_mouse_start_pos = mouse_pos;
        if(selected_layer != nullptr) {
            std::vector<int> entity_indices;
            mouse_world_pos = board_pos_from_window_pos(mouse_pos);
            selected_layer->layer.find_entities_at_point(mouse_world_pos, entity_indices);
            if(entity_indices != active_entities) {
                active_entity_index = 0;
            }
            active_entities = entity_indices;
            active_entity = nullptr;
            selected_layer->layer.clear_entity_flags(entity_flags_t::all_select);
            if(!active_entities.empty()) {
                if(active_entity_index < active_entities.size()) {
                    tesselator_entity &e = selected_layer->layer.entities[active_entities[active_entity_index]];
                    e.flags |= entity_flags_t::active;
                    active_entity = &e;
                } else {
                    active_entity = nullptr;
                }
                active_entity_index = (active_entity_index + 1) % (active_entities.size() + 1);
                for(int i : entity_indices) {
                    selected_layer->layer.entities[i].flags |= entity_flags_t::hovered;
                }
            }
        }
    } break;

    case mouse_drag_select:
        zoom_anim = false;
        drag_mouse_cur_pos = mouse_pos;
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
    view_rect = {{0,0}, {10,10}};

    solid_program.init();
    color_program.init();
    layer_program.init();
    textured_program.init();
    arc_program.init();
    line2_program.init();

    overlay.init();

    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &max_multisamples);

    LOG_INFO("MAX GL Multisamples: {}", max_multisamples);

    if(multisample_count > max_multisamples) {
        multisample_count = max_multisamples;
    }

    gerber_load_thread = std::jthread([this](std::stop_token const &st) { load_gerbers(st); });

    load_settings(config_path(app_name, settings_filename));

    return true;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::file_open()
{
    const nfdpathset_t *paths;
    nfdopendialogu8args_t args{};
    nfdresult_t result = NFD_OpenDialogMultipleU8_With(&paths, &args);
    switch(result) {
    case NFD_OKAY: {
        nfdpathsetsize_t path_count;
        if(NFD_PathSet_GetCount(paths, &path_count) == NFD_OKAY) {
            for(nfdpathsetsize_t i = 0; i < path_count; ++i) {
                nfdu8char_t *outPath;
                if(NFD_PathSet_GetPath(paths, i, &outPath) == NFD_OKAY) {
                    next_layer_color += 1;
                    next_layer_color %= std::size(layer_colors);
                    load_gerber(settings::layer_t{ outPath, gl::colorf4(layer_colors[next_layer_color]).to_string(), true, false });
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

//////////////////////////////////////////////////////////////////////

std::optional<std::filesystem::path> gerber_explorer::save_file_dialog()
{
    nfdu8char_t *path;
    nfdresult_t result = NFD_SaveDialogU8(&path, nullptr, 0, nullptr, "settings.json");
    switch(result) {
    case NFD_OKAY:
        return { path };
    case NFD_CANCEL:
        LOG_DEBUG("Cancelled");
        break;
    default:
        LOG_ERROR("Error: {}", NFD_GetError());
        break;
    }
    return std::nullopt;
}

//////////////////////////////////////////////////////////////////////

std::optional<std::filesystem::path> gerber_explorer::load_file_dialog()
{
    nfdu8char_t *path;
    nfdresult_t result = NFD_OpenDialogU8(&path, nullptr, 0, nullptr);
    switch(result) {
    case NFD_OKAY:
        return { path };
    case NFD_CANCEL:
        LOG_DEBUG("Cancelled");
        break;
    default:
        LOG_ERROR("Error: {}", NFD_GetError());
        break;
    }
    return std::nullopt;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_gerbers(std::stop_token const &st)
{
    LOG_CONTEXT("loader", info);
    LOG_DEBUG("Waiting for filenames...");
    while(!st.stop_requested()) {
        loader_semaphore.acquire();
        if(!st.stop_requested()) {
            settings::layer_t layer_to_load;
            {
                std::lock_guard loader_lock(loader_mutex);
                if(gerber_filenames_to_load.empty()) {
                    LOG_WARNING("Huh? Where's the filename?");
                    continue;
                }
                layer_to_load = gerber_filenames_to_load.front();
            }
            gerber_filenames_to_load.pop_front();
            LOG_DEBUG("Loading {}", layer_to_load.filename);
            std::thread(
                [this](settings::layer_t loaded_layer) {    // copy it again!
                    gerber_lib::gerber *g = new gerber_lib::gerber();
                    gerber_lib::gerber_error_code err = g->parse_file(loaded_layer.filename.c_str());
                    if(err == gerber_lib::ok) {
                        gerber_layer *layer = new gerber_layer();
                        layer->index = loaded_layer.index;
                        next_index = std::max(layer->index + 1, next_index);
                        layer->layer.set_gerber(g);
                        layer->layer.layer_program = &layer_program;
                        layer->layer.line2_program = &line2_program;
                        layer->invert = loaded_layer.inverted;
                        layer->visible = loaded_layer.visible;
                        layer->fill_color.from_string(loaded_layer.color);
                        layer->clear_color = gl::colorf4(gl::colors::clear);
                        layer->draw_mode = loaded_layer.draw_mode;
                        layer->name = std::format("{}", std::filesystem::path(g->filename).filename().string());
                        LOG_DEBUG("Finished loading {}, {}", layer->index, loaded_layer.filename);
                        {
                            std::lock_guard loaded_lock(loaded_mutex);
                            loaded_layers.push_back(layer);
                        }
                    } else {
                        LOG_ERROR("Error loading {} ({})", loaded_layer.filename, gerber_lib::get_error_text(err));
                    }
                },
                layer_to_load)
                .detach();
        }
    }
    LOG_DEBUG("Loader is exiting...");
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::select_layer(gerber_layer *layer)
{
    if(selected_layer != nullptr) {
        selected_layer->layer.clear_entity_flags(entity_flags_t::all_select);
    }
    selected_layer = layer;
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::load_gerber(settings::layer_t const &layer)
{
    {
        std::lock_guard lock(loader_mutex);
        gerbers_to_load += 1;
        gerber_filenames_to_load.push_back(layer);    // copy it!
    }
    loader_semaphore.release();
    std::this_thread::yield();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::ui()
{
    // ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Open Layers", "Ctrl-O", nullptr)) {
                file_open();
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Save Settings", "Ctrl-S", nullptr)) {
                auto save_path = save_file_dialog();
                if(save_path.has_value()) {
                    save_settings(save_path.value());
                }
            }
            if(ImGui::MenuItem("Load Settings", "Ctrl-L", nullptr)) {
                auto load_path = load_file_dialog();
                if(load_path.has_value()) {
                    load_settings(load_path.value());
                }
            }
            // ImGui::MenuItem("Stats", nullptr, &show_stats);
            // ImGui::MenuItem("Options", nullptr, &show_options);
            ImGui::Separator();
            if(ImGui::MenuItem("Close all", nullptr, nullptr)) {
                while(!layers.empty()) {
                    auto l = layers.front();
                    layers.pop_front();
                    delete l;
                }
                select_layer(nullptr);
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Exit", "Esc", nullptr)) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View")) {
            if(ImGui::MenuItem("Fit to window", "F", nullptr, !layers.empty())) {
                fit_to_window();
            }
            ImGui::MenuItem("Flip X", "X", &settings.flip_x);
            ImGui::MenuItem("Flip Y", "Y", &settings.flip_y);
            ImGui::MenuItem("Wireframe", "W", &settings.wireframe);
            ImGui::MenuItem("Show Axes", "A", &settings.show_axes);
            ImGui::MenuItem("Show Extent", "E", &settings.show_extent);
            if(ImGui::BeginMenu("Outline")) {
                ImGui::SliderFloat("##val", &settings.outline_width, 0.0f, 8.0f, "%.1f");
                ImGui::ColorEdit4("Outline color",
                                  (float *)&settings.outline_color,
                                  ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();

    gerber_layer *item_to_move = nullptr;
    gerber_layer *item_target = nullptr;
    gerber_layer *item_to_delete = nullptr;
    bool move_before = false;

    ImGui::Begin("Files");
    {
        bool any_item_hovered = false;
        float controls_width = ImGui::GetFrameHeight() * 5 + ImGui::GetStyle().ItemSpacing.x * 5;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));

        if(ImGui::BeginTable("Layers", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, controls_width);

            for(auto it = layers.rbegin(); it != layers.rend(); ++it) {
                gerber_layer *l = *it;
                ImGui::PushID(l);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImVec2 row_min = ImGui::GetCursorScreenPos();    // Capture start of row
                float row_height = ImGui::GetFrameHeight();
                ImVec2 row_size = ImVec2(ImGui::GetContentRegionAvail().x + controls_width, row_height);
                ImVec2 row_max = ImVec2(row_min.x + row_size.x, row_min.y + row_size.y);
                float mid_y = row_min.y + (row_height / 2.0f);
                bool is_selected = (selected_layer == l);

                auto flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;

                ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                if(!l->visible) {
                    color.w *= 0.5f;
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);

                if(ImGui::Selectable(l->name.c_str(), is_selected, flags, ImVec2(0, row_height))) {
                    select_layer(l);
                }

                ImGui::PopStyleColor();

                if(ImGui::IsItemHovered()) {
                    any_item_hovered = true;
                }

                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
                    ImGui::SetDragDropPayload("GERBER_LAYER", &l, sizeof(gerber_layer *));
                    ImGui::Text("%s", l->name.c_str());
                    ImGui::EndDragDropSource();
                }

                ImRect top_half(row_min, ImVec2(row_max.x, mid_y));
                ImRect bot_half(ImVec2(row_min.x, mid_y), row_max);

                auto DoDropTarget = [&](ImRect rect, bool is_before, ImGuiID id) {
                    if(ImGui::BeginDragDropTargetCustom(rect, id)) {
                        if(ImGui::IsDragDropActive()) {
                            float line_y = is_before ? row_min.y : row_max.y;
                            ImU32 col = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
                            ImGui::GetForegroundDrawList()->AddLine(ImVec2(row_min.x, line_y), ImVec2(row_max.x, line_y), col, 2.5f);
                        }

                        if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("GERBER_LAYER")) {
                            item_to_move = *(gerber_layer **)payload->Data;
                            item_target = l;
                            move_before = is_before;
                        }
                        ImGui::EndDragDropTarget();
                    }
                };

                DoDropTarget(top_half, true, ImGui::GetID("##top"));
                DoDropTarget(bot_half, false, ImGui::GetID("##bot"));

                ImGui::TableSetColumnIndex(1);
                IconCheckbox("##vis", &l->visible, MATSYM_visibility, MATSYM_visibility_off);
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetItemTooltip("Toggle visibility");
                }
                ImGui::SameLine();
                IconCheckbox("##inv", &l->invert, MATSYM_invert_colors, MATSYM_invert_colors_off);
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetItemTooltip("Invert layer");
                }
                ImGui::SameLine();
                IconCheckboxTristate("##mode", &l->draw_mode, MATSYM_radio_button_checked, MATSYM_circle, MATSYM_radio_button_off);
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetItemTooltip("Solid/Mixed/Outline only");
                }
                ImGui::SameLine();
                ImGui::ColorEdit4("##clr",
                                  l->fill_color.f,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
                ImGui::SameLine();
                if(IconButton("##del", MATSYM_close)) {
                    item_to_delete = l;
                }
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetItemTooltip("Delete layer");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        if(item_to_delete) {
            layers.erase(std::remove(layers.begin(), layers.end(), item_to_delete), layers.end());
            delete item_to_delete;
            select_layer(nullptr);
        } else if(item_to_move && item_target && item_to_move != item_target && item_to_move != item_target) {
            auto dragged_it = std::find(layers.begin(), layers.end(), item_to_move);
            auto target_it = std::find(layers.begin(), layers.end(), item_target);
            if(dragged_it != layers.end() && target_it != layers.end()) {
                if(move_before) {
                    target_it = std::next(target_it);
                }
                layers.splice(target_it, layers, dragged_it);
            }
        }

        if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !any_item_hovered) {
            select_layer(nullptr);
        }
    }
    ImGui::End();

    ImGui::Begin("Info");
    if(active_entity != nullptr) {
        ImGui::Text("%d", active_entity->entity_id);
    } else {
        ImGui::Text("...");
    }
    ImGui::End();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::update_board_extent()
{
    rect all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
    for(auto layer : layers) {
        if(layer->visible) {
            all = all.union_with(layer->extent());
        }
    }
    board_extent = all;
    board_center = all.center();
}

//////////////////////////////////////////////////////////////////////

void gerber_explorer::blend_layer(gl::colorf4 const &col_r, gl::colorf4 const &col_g, gl::colorf4 const &col_b, float alpha)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    GL_CHECK(glViewport(viewport_xpos, window_height - (viewport_height + viewport_ypos), viewport_width, viewport_height));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    textured_program.use();
    layer_render_target.bind_textures();

    GL_CHECK(glUniform4fv(textured_program.u_red, 1, col_r.f));
    GL_CHECK(glUniform4fv(textured_program.u_green, 1, col_g.f));
    GL_CHECK(glUniform4fv(textured_program.u_blue, 1, col_b.f));
    GL_CHECK(glUniform1f(textured_program.u_alpha, alpha));
    GL_CHECK(glUniform1i(textured_program.u_num_samples, layer_render_target.num_samples));
    GL_CHECK(glUniform1i(textured_program.u_cover_sampler, 0));

    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));
    GL_CHECK(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
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
            loaded_layer->layer.on_finished_loading();
            layers.push_front(loaded_layer);
            layers.sort([](gerber_layer const *a, gerber_layer const *b) { return a->index > b->index; });
            {
                LOG_INFO("Loaded layer \"{}\"", loaded_layer->filename());
                std::lock_guard lock(loader_mutex);
                if(gerbers_to_load != 0) {
                    gerbers_to_load -= 1;
                    if(gerbers_to_load == 0) {
                        select_layer(nullptr);
                        fit_to_window();
                    }
                }
            }
        }
    }

    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGuiDockNode *central_node = ImGui::DockBuilderGetCentralNode(dockspace_id);

    rect old_viewport_rect = viewport_rect;

    if(central_node) {
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        ImVec2 pos = ImVec2(central_node->Pos.x - main_viewport->Pos.x, central_node->Pos.y - main_viewport->Pos.y);
        ImVec2 size = central_node->Size;
        viewport_xpos = (int)pos.x;
        viewport_ypos = (int)pos.y;
        viewport_width = (int)size.x;
        viewport_height = (int)size.y;
    } else {
        // this should never happen
        viewport_xpos = 0;
        viewport_ypos = 0;
        viewport_width = window_width;
        viewport_width = window_height;
    }
    viewport_rect = rect(vec2d(viewport_xpos, viewport_ypos), vec2d(viewport_xpos + viewport_width, viewport_ypos + viewport_height));
    viewport_size = viewport_rect.size();

    bool viewport_changed = viewport_rect.min_pos.x != old_viewport_rect.min_pos.x || viewport_rect.min_pos.y != old_viewport_rect.min_pos.y ||
                            viewport_rect.max_pos.x != old_viewport_rect.max_pos.x || viewport_rect.max_pos.y != old_viewport_rect.max_pos.y;

    vec2d new_viewport_size;
    new_viewport_size.x = viewport_width;
    new_viewport_size.y = viewport_height;

    vec2d scale_factor = new_viewport_size.divide(viewport_size);

    vec2d new_view_size = view_rect.size().multiply(scale_factor);
    view_rect.max_pos = view_rect.min_pos.add(new_view_size);

    if(viewport_changed) {
        if(should_fit_to_window) {
            fit_to_window();
            view_rect = target_view_rect;
            zoom_anim = false;
        } else {
            // viewport dimensions changed, updated view_rect accordingly
            vec2d scale = old_viewport_rect.size().divide(viewport_rect.size());
            view_rect.max_pos = view_rect.min_pos.add(view_rect.size().divide(scale));
            zoom_anim = false;
        }
    }

    int framebuffer_width;
    int framebuffer_height;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

    handle_mouse();

    glLineWidth(1.0f);

    ui();

    update_view_rect();

    if(window_width == 0 || window_height == 0) {
        return;
    }

    view_scale = viewport_size.divide(view_rect.size());

    // get bounding rect/center of all layers (for flip)
    update_board_extent();

    flip_xy = { settings.flip_x ? -1.0 : 1.0, settings.flip_y ? -1.0 : 1.0 };

    ortho_screen_matrix = make_ortho(viewport_width, viewport_height);

    // world matrix has optional x/y flip around board center
    gl_matrix flip_m = make_identity();
    flip_m.m[0] = (float)flip_xy.x;
    flip_m.m[5] = (float)flip_xy.y;
    flip_m.m[12] = (float)(board_center.x - flip_xy.x * board_center.x);
    flip_m.m[13] = (float)(board_center.y - flip_xy.y * board_center.y);
    gl_matrix view_m = make_identity();
    view_m.m[0] = (float)view_scale.x;
    view_m.m[5] = (float)view_scale.y;
    view_m.m[12] = (float)(-(view_rect.min_pos.x * view_scale.x));
    view_m.m[13] = (float)(-(view_rect.min_pos.y * view_scale.y));
    gl_matrix temp = matrix_multiply(view_m, flip_m);
    world_matrix = matrix_multiply(ortho_screen_matrix, temp);

    //////////////////////////////////////////////////////////////////////
    // Draw stuff

    GL_CHECK(glClearColor(0, 0, 0, 1.0f));
    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glDisable(GL_CULL_FACE));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    // resize the offscreen render target if the window size changed

    if(layer_render_target.width != viewport_width || layer_render_target.height != viewport_height || layer_render_target.num_samples != multisample_count) {
        layer_render_target.cleanup();
        layer_render_target.init(viewport_width, viewport_height, multisample_count, 1);
    }

    // draw the layers

    for(auto r = layers.begin(); r != layers.end(); ++r) {
        gerber_layer &layer = **r;

        if(layer.visible) {
            layer_render_target.bind_framebuffer();
            GL_CHECK(glViewport(0, 0, viewport_width, viewport_height));

            if(settings.wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            gl::colorf4 fill = layer.fill_color;
            gl::colorf4 clear = layer.clear_color;
            gl::colorf4 f(gl::colors::black);
            if(layer.invert) {
                f = gl::colorf4(gl::colors::green);
                std::swap(fill, clear);
            }
            glClearColor(f.red(), f.green(), f.blue(), f.alpha());
            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
            layer.layer.fill(world_matrix, entity_flags_t::selected, entity_flags_t::clear, entity_flags_t::fill);
            blend_layer(gl::colorf4(1, 1, 1, 1), clear, fill, layer.alpha / 255.0f);
        }
    }

    // draw overlay/ouline of selected & hovered entities in selected layer on top of all other layers

    if(selected_layer != nullptr) {
        layer_render_target.bind_framebuffer();
        GL_CHECK(glViewport(0, 0, viewport_width, viewport_height));
        GL_CHECK(glClearColor(0, 0, 0, 1));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
        gl::color red_fill = gl::colors::red;
        gl::color green_fill = gl::colors::green & 0x80ffffff;
        gl::color blue_fill = gl::colors::blue & 0x40ffffff;
        selected_layer->layer.fill(world_matrix, entity_flags_t::active, entity_flags_t::selected, entity_flags_t::hovered, red_fill, green_fill, blue_fill);
        gl::colorf4 active(gl::colors::white);
        gl::colorf4 selected(gl::colors::silver);
        gl::colorf4 hovered(gl::colors::gray);
        blend_layer(active, selected, hovered, 0.6666f);

        // Draw outline for hovered/selected entities in the selected layer
        if(settings.outline_width > 0.0f) {
            layer_render_target.bind_framebuffer();
            GL_CHECK(glViewport(0, 0, viewport_width, viewport_height));
            GL_CHECK(glClearColor(0, 0, 0, 0));
            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
            selected_layer->layer.outline(settings.outline_width, world_matrix, window_size);
            blend_layer(gl::colorf4(gl::colors::white), gl::colorf4(gl::colors::clear), gl::colorf4(gl::colors::black), 1.0f);
        }
    }

    // draw the overlay graphics

    overlay.reset();

    if(mouse_mode == mouse_drag_zoom_select) {
        rect drag_rect_corrected = correct_aspect_ratio(view_rect.aspect_ratio(), drag_rect, aspect_expand);
        overlay.add_rect(drag_rect_corrected, 0x80ffff00);
        overlay.add_rect(drag_rect, 0x800000ff);
        overlay.add_outline_rect(drag_rect, 0xffffffff);
    }

    vec2d origin = window_pos_from_world_pos({ 0, 0 });

    gl::color axes_color = gl::colors::cyan;
    gl::color extent_color = gl::colors::yellow;

    if(settings.show_axes) {
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
        for(auto const &e : selected_layer->layer.entities) {
            if(e.flags & entity_flags_t::selected) {
                rect const &b = e.bounds;
                rect w{ window_pos_from_world_pos(b.min_pos), window_pos_from_world_pos(b.max_pos) };
                overlay.add_outline_rect(w, extent_color);
            }
        }
    }

    if(mouse_mode == mouse_drag_select) {
        rect f{ drag_mouse_start_pos, drag_mouse_cur_pos };
        uint32_t color = 0x40ff8020;
        if(f.min_pos.x > f.max_pos.x) {
            color = 0x4080ff20;
        }
        overlay.add_rect(f, color);
        overlay.add_outline_rect(f, 0xffffffff);
    }

    rect ext{ window_pos_from_world_pos(arc_extent.min_pos), window_pos_from_world_pos(arc_extent.max_pos) };
    overlay.add_outline_rect(ext, gl::colors::green);

    color_program.use();

    GL_CHECK(glUniformMatrix4fv(color_program.u_transform, 1, false, ortho_screen_matrix.m));

    overlay.draw();
}
