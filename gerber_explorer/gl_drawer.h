//////////////////////////////////////////////////////////////////////

#pragma once

#include "gl_base.h"
#include "gerber_lib.h"
#include "gerber_draw.h"

struct TESStesselator;

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    enum draw_call_flags
    {
        draw_call_flag_none = 0,
        draw_call_flag_clear = 1
    };

    //////////////////////////////////////////////////////////////////////

    struct tesselator_entity
    {
        int entity_id;                           // redundant, strictly speaking, but handy
        int first_outline;                       // offset into outlines spans
        int first_fill;                          // offset into fills spans
        int num_outlines{};                      // # of boundary outline line loops
        int num_fills{};                         // # of fill draw calls
        draw_call_flags flags;                   // clear/fill
        gerber_lib::gerber_2d::rect bounds{};    // for picking speedup
    };

    struct tesselator_span
    {
        int start;    // glDrawArrays(start, length) (boundary lines) or glDrawElements(start, length) (interior triangles)
        int length;
    };

    struct gl_tesselator
    {
        using vec2f = gerber_lib::gerber_2d::vec2f;
        using vert = gl_vertex_solid;

        TESStesselator *boundary_stesselator{};

        std::vector<tesselator_entity> entities;
        std::vector<vec2f> points;
        std::vector<vert> vertices;
        std::vector<GLuint> indices;
        std::vector<tesselator_span> boundaries;
        std::vector<tesselator_span> fills;
        std::vector<vec2f> outline_vertices;

        void clear();
        void new_entity(int entity_id, draw_call_flags flags);
        void append_points(size_t offset);
        void finish_entity();
        void finalize();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        using vec2f = gerber_lib::gerber_2d::vec2f;

        gl_drawer() = default;

        void set_gerber(gerber_lib::gerber *g) override;
        void on_finished_loading() override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;
        // void finish_element(int entity_id) override;
        void draw(bool fill, bool outline, bool wireframe, float outline_thickness);

        gl_tesselator tesselator;

        gerber_lib::gerber *gerber_file{};
        draw_call_flags current_flag{ draw_call_flag_none };

        int base_vert{};
        int current_entity_id{ -1 };

        TESStesselator *tess{};
        gl_solid_program *program{};

        // all the verts for boundaries and interiors
        gl_vertex_array_solid vertex_array;

        // indices for interior triangles
        gl_index_array index_array;
    };

}    // namespace gerber_3d
