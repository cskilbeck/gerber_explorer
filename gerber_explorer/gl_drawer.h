//////////////////////////////////////////////////////////////////////

#pragma once

#include "gl_base.h"
#include "gerber_lib.h"
#include "gerber_draw.h"
#include "gl_matrix.h"

struct TESStesselator;

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    namespace entity_flags_t
    {
        int constexpr none = 0;
        int constexpr clear = (1 << 0);
        int constexpr hovered = (1 << 1);
        int constexpr selected = (1 << 2);
    }    // namespace entity_flags_t

    //////////////////////////////////////////////////////////////////////

    struct tesselator_entity
    {
        int entity_id;                           // redundant, strictly speaking, but handy
        int first_fill;                          // offset into fills spans
        int num_fills{};                         // # of fill draw calls
        int outline_offset;                      // offset into outline_vertices_start/end
        int outline_size{};                      // # of verts in the outline
        int flags;                               // clear/fill/hover/select
        gerber_lib::gerber_2d::rect bounds{};    // for picking speedup
    };

    struct tesselator_span
    {
        int start;    // glDrawElements(start, length) (for interior triangles)
        int length;
    };

    struct gl_tesselator
    {
        using vec2f = gerber_lib::gerber_2d::vec2f;
        using vert = vec2f;

        TESStesselator *boundary_stesselator{};

        std::vector<tesselator_entity> entities;

        std::vector<vec2f> points;

        std::vector<vert> fill_vertices;
        std::vector<vec2f> outline_vertices_start;
        std::vector<vec2f> outline_vertices_end;

        std::vector<GLuint> indices;
        std::vector<tesselator_span> fills;

        int contours{};

        void clear();
        void new_entity(int entity_id, int flags);
        void append_points(size_t offset);
        void finish_entity();
        void finalize();

        void flag_entities_at_point(gerber_lib::gerber_2d::vec2d point, int clear_flags, int set_flags);
        void flag_touching_entities(gerber_lib::gerber_2d::rect const &world_rect, int clear_flags, int set_flags);
        void flag_enclosed_entities(gerber_lib::gerber_2d::rect const &world_rect, int clear_flags, int set_flags);
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        using vec2f = gerber_lib::gerber_2d::vec2f;

        gl_drawer() = default;

        void set_gerber(gerber_lib::gerber *g) override;
        void on_finished_loading() override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;
        void draw(bool fill, bool outline, bool wireframe, float outline_thickness, bool invert, gl_matrix const &matrix,
                  gerber_lib::gerber_2d::vec2d const &window_size);

        gl_tesselator tesselator;

        gerber_lib::gerber *gerber_file{};
        int current_flag{ entity_flags_t::none };
        int base_vert{};
        int current_entity_id{ -1 };

        gl_layer_program *layer_program{};
        gl_line_program *line_program{};

        // all the verts for interiors
        gl_vertex_array_solid vertex_array;

        // indices for interior triangles
        gl_index_buffer index_array;

        // vertices for outline (2 because start, end)
        GLuint lines_vbo[2];
    };

}    // namespace gerber_3d
