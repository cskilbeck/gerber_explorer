//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>

#include "gl_base.h"
#include "gerber_lib.h"
#include "gerber_draw.h"
#include "gerber_net.h"
#include "gerber_arena.h"
#include "gl_matrix.h"

#include "tesselator.h"

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

    using tesselation_quality_t = unsigned int;

    namespace tesselation_quality
    {
        tesselation_quality_t constexpr low = 0;
        tesselation_quality_t constexpr medium = 1;
        tesselation_quality_t constexpr high = 2;
        tesselation_quality_t constexpr num_qualities = 3;
    };    // namespace tesselation_quality

    //////////////////////////////////////////////////////////////////////

    static char const *tesselation_quality_name(tesselation_quality_t q)
    {
        static constexpr char const *names[tesselation_quality::num_qualities] = { "Low", "Medium", "High" };
        if(q < std::size(names)) {
            return names[q];
        }
        return "?Unknown";
    }

    //////////////////////////////////////////////////////////////////////

    struct tess_arena_t : gerber_lib::gerber_arena<1ULL << 30, 16>
    {
        tess_arena_t() : gerber_arena()
        {
            memset(&tess_alloc, 0, sizeof(tess_alloc));
            tess_alloc.memalloc = tess_allocate;
            tess_alloc.memfree = tess_free;
            tess_alloc.userData = (void *)this;
        }

        static void *tess_allocate(void *userData, unsigned int size)
        {
            auto arena = (tess_arena_t *)userData;
            return arena->alloc(size);
        }

        static void tess_free(void *userData, void *ptr)
        {
            (void)userData;
            (void)ptr;
        }

        TESSalloc tess_alloc{};
    };

    //////////////////////////////////////////////////////////////////////

    struct tesselator_entity
    {
        gerber_lib::gerber_net *net{};
        int fill_index;               // offset into fills spans
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
        template <typename T> using typed_arena = gerber_lib::typed_arena<T>;

        gl_drawer() = default;

        // setup from a parsed gerber file
        void set_gerber(gerber_lib::gerber *g) override;

        // setup anything that has to be done in the main thread
        void create_gl_resources();

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
        void fill(gl_matrix const &matrix, uint8_t r_flags, uint8_t g_flags, uint8_t b_flags);

        void outline(float outline_thickness, gl_matrix const &matrix, gerber_lib::vec2d const &viewport_size);

        // picking/selection
        void clear_entity_flags(int flags);
        int flag_entities_at_point(gerber_lib::vec2d point, int clear_flags, int set_flags);
        int flag_touching_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        int flag_enclosed_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        void find_entities_at_point(gerber_lib::vec2d point, std::vector<int> &indices);
        void select_hovered_entities();

        void release();

        bool ready_to_draw{ false };

        void update_flags_buffer();

        gerber_lib::gerber *gerber_file{};
        tesselation_quality_t tesselation_quality;
        int current_flag{ entity_flags_t::none };
        int base_vert{};
        int current_entity_id{ -1 };
        TESStesselator *boundary_stesselator{};
        tess_arena_t boundary_arena;
        tess_arena_t interior_arena;
        typed_arena<tesselator_entity> entities;
        typed_arena<vec2f> temp_points{};
        typed_arena<gl_line2_program::line> outline_lines{};
        typed_arena<vec2f> outline_vertices{};
        typed_arena<uint8_t> entity_flags;    // one byte per entity
        typed_arena<gl_vertex_entity> fill_vertices;
        typed_arena<GLuint> fill_indices;
        gl_vertex_array_entity vertex_array;
        gl_index_buffer index_array;

        GLuint outline_lines_buffer;
        GLuint outline_vertices_buffer;
        GLuint flags_buffer;

        GLuint outline_lines_texture;
        GLuint outline_vertices_texture;
        GLuint flags_texture;
    };

}    // namespace gerber_3d
