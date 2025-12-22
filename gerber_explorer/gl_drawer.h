//////////////////////////////////////////////////////////////////////

#pragma once

#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_draw.h"

#include "gl_window.h"

#include "tesselator.h"

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        enum draw_call_flags
        {
            draw_call_flag_clear = 1
        };

        void set_gerber(gerber_lib::gerber *g) override;
        void on_finished_loading() override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        void draw(bool fill, bool outline, bool wireframe, float outline_thickness);
        void fill_entities(std::list<tesselator_entity const *> const &entities);

        gerber_lib::gerber *gerber_file{};

        std::vector<gl_vertex_solid> vertices;    // all the verts for all the entities in this file
        std::vector<GLuint> triangle_indices;

        gl_vertex_array_solid vertex_array;
        gl_index_array indices_triangles;

        gl_layer_program *program{};

        gl_drawer() = default;

        int current_entity_id{ -1 };
        uint32_t current_flag;
    };

}    // namespace gerber_3d
