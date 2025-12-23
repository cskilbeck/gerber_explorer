//////////////////////////////////////////////////////////////////////

#pragma once

#include "gl_base.h"
#include "gerber_lib.h"
#include "gerber_draw.h"

struct TESStesselator;

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct vertex_span
    {
        uint32_t start;
        uint32_t length;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        gl_drawer() = default;

        enum draw_call_flags
        {
            draw_call_flag_none = 0,
            draw_call_flag_clear = 1
        };

        void set_gerber(gerber_lib::gerber *g) override;
        void on_finished_loading() override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;
        void finish_element(int entity_id) override;
        void render(uint32_t color);
        void render_triangle(int draw_call_index, int triangle_index, uint32_t color);

        gerber_lib::gerber *gerber_file{};
        int current_entity_id{-1};
        draw_call_flags current_flag{draw_call_flag_none};
        std::vector<gl_vertex_solid> vertices;    // all the verts for all the entities in this file
        std::vector<GLuint> indices;
        std::vector<vertex_span> draw_calls;    // start, length

        int base_vert{};

        struct vec2f
        {
            float x;
            float y;
        };

        std::vector<vec2f> points;

        static bool is_clockwise(std::vector<vec2f> const &points, size_t start, size_t end);
        TESStesselator *tess{};
        gl_solid_program *program{};
        gl_vertex_array_solid vertex_array;
        gl_index_array index_array;

    };

}    // namespace gerber_3d
