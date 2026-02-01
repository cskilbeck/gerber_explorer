//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>

#include "gl_base.h"
#include "gerber_lib.h"
#include "gerber_draw.h"
#include "gerber_net.h"
#include "gerber_arena.h"
#include "gl_matrix.h"

#include "clipper2/clipper.h"

#include "tesselator.h"

#include "gerber_log.h"

struct gerber_layer;

namespace gerber
{
    //////////////////////////////////////////////////////////////////////

    template <typename T> using typed_arena = gerber_lib::typed_arena<T>;

    namespace entity_flags_t
    {
        uint8_t constexpr none = 0;
        uint8_t constexpr clear = (1 << 0);       // remove material
        uint8_t constexpr fill = (1 << 1);        // add material
        uint8_t constexpr hovered = (1 << 2);     // mouse hovering over it
        uint8_t constexpr selected = (1 << 3);    // it's selected
        uint8_t constexpr active = (1 << 4);      // there can be only one active entity (click cycles through entities under the mouse position)

        uint8_t constexpr all_select = hovered | selected | active;
    }    // namespace entity_flags_t

    //////////////////////////////////////////////////////////////////////

    using tesselation_quality_t = unsigned int;

    namespace tesselation_quality
    {
        tesselation_quality_t constexpr low = 0;
        tesselation_quality_t constexpr medium = 1;
        tesselation_quality_t constexpr high = 2;
        tesselation_quality_t constexpr num_qualities = 3;
    }    // namespace tesselation_quality

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

    //////////////////////////////////////////////////////////////////////

    template <typename vertex_array_type> struct drawable_shape
    {
        LOG_CONTEXT("drawable", debug);

        using vertex_type = typename vertex_array_type::vertex_type;

        typed_arena<vertex_type> vertices;
        typed_arena<GLuint> indices;
        vertex_array_type vertex_array;
        gl::index_buffer index_array;
        bool ready_to_draw{ false };

        // init cpu buffers
        void init()
        {
            LOG_INFO("init");
            vertices.init();
            indices.init();
        }

        // release cpu buffers
        void release()
        {
            LOG_INFO("release");
            vertices.release();
            indices.release();
            ready_to_draw = false;
        }

        // setup GPU buffers
        void create_gpu_resources()
        {
            if(!ready_to_draw) {
                LOG_INFO("create_gpu_resources");
                cleanup();
                if(!vertices.empty() && !indices.empty()) {
                    GL_CHECK(vertex_array.init(vertices.size()));
                    GL_CHECK(index_array.init(indices.size()));
                    update();
                }
            }
        }

        // release GPU buffers
        void cleanup()
        {
            LOG_INFO("cleanup");
            vertex_array.cleanup();
            index_array.cleanup();
            ready_to_draw = false;
        }

        // update cpu data -> GPU buffers
        void update()
        {
            LOG_INFO("update, vertices.size() = {}", vertices.size());
            if(!vertices.empty()) {
                GL_CHECK(vertex_array.activate());
                GL_CHECK(gl::update_buffer<GL_ARRAY_BUFFER>(vertices));
            }

            if(!indices.empty()) {
                GL_CHECK(index_array.activate());
                GL_CHECK(gl::update_buffer<GL_ELEMENT_ARRAY_BUFFER>(indices));
            }
            ready_to_draw = true;
        }

        // render it
        void draw()
        {
            GL_CHECK(vertex_array.activate());
            GL_CHECK(index_array.activate());
            GL_CHECK(glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, nullptr));
        }
    };

    using solid_shape = drawable_shape<gl::vertex_array_solid>;

    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        using vec2f = gerber_lib::vec2f;
        using vert = vec2f;

        gl_drawer() = default;

        void init(gerber_layer const *for_layer)
        {
            layer = for_layer;
            boundary_arena.init();
            interior_arena.init();
            entities.init();
            temp_points.init();
            outline_lines.init();
            outline_vertices.init();
            entity_flags.init();
            fill_vertices.init();
            fill_indices.init();
        }

        // setup from a parsed gerber file
        void set_gerber(gerber_lib::gerber_file *g) override;

        // setup anything that has to be done in the main thread
        void create_gl_resources();
        void release_gl_resources();

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
        void fill(gl::matrix const &matrix, uint8_t r_flags, uint8_t g_flags, uint8_t b_flags, uint8_t draw_flags);

        void outline(float outline_thickness, gl::matrix const &matrix, gerber_lib::vec2d const &viewport_size);

        // picking/selection
        void clear_entity_flags(int flags);
        int flag_entities_at_point(gerber_lib::vec2d point, int clear_flags, int set_flags);
        int flag_touching_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        int flag_enclosed_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        void find_entities_at_point(gerber_lib::vec2d point, std::vector<int> &indices);
        void select_hovered_entities();

        void release();

        void create_mask();
        bool got_mask{ false };

        bool ready_to_draw{ false };

        void update_flags_buffer();

        std::string const &name() const;

        // only used if it's an outline layer
        solid_shape mask{};

        gerber_layer const * layer{};
        tesselation_quality_t tesselation_quality;
        int current_flag{ entity_flags_t::none };
        int base_vert{};
        int current_entity_id{ -1 };
        TESStesselator *boundary_stesselator{};
        tess_arena_t boundary_arena;
        tess_arena_t interior_arena;
        typed_arena<tesselator_entity> entities;
        typed_arena<vec2f> temp_points;
        typed_arena<gl::line2_program::line> outline_lines;
        typed_arena<vec2f> outline_vertices;
        typed_arena<uint8_t> entity_flags;    // one byte per entity
        typed_arena<gl::vertex_entity> fill_vertices;
        typed_arena<GLuint> fill_indices;
        gl::vertex_array_entity vertex_array;
        gl::index_buffer index_array;

        GLuint outline_lines_buffer{};
        GLuint outline_vertices_buffer{};
        GLuint flags_buffer{};

        GLuint outline_lines_texture{};
        GLuint outline_vertices_texture{};
        GLuint flags_texture{};
    };

}    // namespace gerber
