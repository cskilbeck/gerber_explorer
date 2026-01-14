//////////////////////////////////////////////////////////////////////

#pragma once

#include "gl_base.h"
#include "gerber_lib.h"
#include "gerber_draw.h"
#include "gerber_net.h"
#include "gl_matrix.h"

struct TESStesselator;

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    namespace entity_flags_t
    {
        int constexpr none = 0;
        int constexpr clear = (1 << 0);       // remove material
        int constexpr fill = (1 << 1);        // add material
        int constexpr hovered = (1 << 2);     // mouse hovering over it
        int constexpr selected = (1 << 3);    // it's selected
        int constexpr active = (1 << 4);      // there can be only one active entity (click cycles through entities under the mouse position)

        int constexpr all_select = hovered | selected | active;
    }    // namespace entity_flags_t

    //////////////////////////////////////////////////////////////////////

    struct tesselator_entity
    {
        gerber_lib::gerber_net *net{};
        int first_fill;               // offset into fills spans
        int num_fills{};              // # of fill draw calls
        int outline_offset;           // offset into outline lines
        int outline_size{};           // # of lines in the outline
        int flags;                    // see entity_flags_t
        gerber_lib::rect bounds{};    // for picking speedup

        int entity_id() const
        {
            return net->entity_id;
        }
    };

    struct tesselator_span
    {
        int start;    // glDrawElements(start, length) (for interior triangles)
        int length;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        using vec2f = gerber_lib::vec2f;
        using vert = vec2f;

        gl_drawer() = default;

        // setup from a parsed gerber file
        void set_gerber(gerber_lib::gerber *g) override;

        // setup anything that has to be done in the main thread
        void on_finished_loading() override;

        // callback to create draw calls from elements
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity,
                           gerber_lib::gerber_net *gnet) override;

        // admin for tesselation etc
        void clear();
        void new_entity(gerber_lib::gerber_net *net, int flags);
        void append_points(size_t offset);
        void finish_entity();
        void finalize();

        // for actually drawing it
        void fill(gl_matrix const &matrix, uint8_t r_flags, uint8_t g_flags, uint8_t b_flags, gl::color red_fill = gl::colors::red,
                  gl::color green_fill = gl::colors::green, gl::color blue_fill = gl::colors::blue);

        void outline(float outline_thickness, gl_matrix const &matrix, gerber_lib::vec2d const &viewport_size);

        // picking/selection
        void clear_entity_flags(int flags);
        int flag_entities_at_point(gerber_lib::vec2d point, int clear_flags, int set_flags);
        int flag_touching_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        int flag_enclosed_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        void find_entities_at_point(gerber_lib::vec2d point, std::vector<int> &indices);
        void select_hovered_entities();

        gerber_lib::gerber *gerber_file{};
        int current_flag{ entity_flags_t::none };
        int base_vert{};
        int current_entity_id{ -1 };

        // tesselation
        TESStesselator *boundary_stesselator{};
        std::vector<tesselator_entity> entities;

        std::vector<vec2f> temp_points;

        std::vector<gl_line2_program::line> outline_lines;
        std::vector<vec2f> outline_vertices;
        std::vector<uint8_t> entity_flags;    // one byte per entity

        std::vector<vert> fill_vertices;
        std::vector<GLuint> fill_indices;
        std::vector<tesselator_span> fill_spans;

        // drawing
        gl_layer_program *layer_program{};
        // gl_line_program *line_program{};
        gl_line2_program *line2_program{};

        // all the verts for interiors
        gl_vertex_array_solid vertex_array;

        // indices for interior triangles
        gl_index_buffer index_array;

        GLuint line_buffers[3];
        GLuint textures[3];
    };

}    // namespace gerber_3d
