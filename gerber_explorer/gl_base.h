#pragma once

#include <cstdint>

#include "gerber_log.h"
#include "gerber_2d.h"
#include "gl_colors.h"

#include "cpptrace/cpptrace.hpp"

//////////////////////////////////////////////////////////////////////

extern int gl_log;

#if defined(_DEBUG) || 1
#define GL_CHECK(x)                                                                                                                    \
    do {                                                                                                                               \
        if(gl_log != 0) {                                                                                                              \
            LOG_INFO("{}", #x);                                                                                                        \
        }                                                                                                                              \
        x;                                                                                                                             \
        GLenum __err = glGetError();                                                                                                   \
        if(__err != 0) {                                                                                                               \
            LOG_ERROR("ERROR {} from {} at line {} of {}\n{}", __err, #x, __LINE__, __FILE__, cpptrace::generate_trace().to_string()); \
            fflush(stdout);                                                                                                            \
        }                                                                                                                              \
    } while(0)
#else
#define GL_CHECK(x) \
    do {            \
        x;          \
    } while(0)
#endif

#define LOG_GL(...)                \
    do {                           \
        if(gl_log != 0) {          \
            LOG_INFO(__VA_ARGS__); \
        }                          \
    } while(false)

inline void set_uniform_1i(int id, int v)
{
    LOG_CONTEXT("set_uniform_1i", error);
    if(id != -1) {
        GL_CHECK(glUniform1i(id, v));
    }
}

inline void set_uniform_1ui(int id, GLuint v)
{
    LOG_CONTEXT("set_uniform_1i", error);
    if(id != -1) {
        GL_CHECK(glUniform1ui(id, v));
    }
}

inline void set_uniform_1f(int id, float v)
{
    LOG_CONTEXT("set_uniform_1f", error);
    if(id != -1) {
        GL_CHECK(glUniform1f(id, v));
    }
}

inline void set_uniform_2f(int id, float x, float y)
{
    LOG_CONTEXT("set_uniform_2f", error);
    if(id != -1) {
        GL_CHECK(glUniform2f(id, x, y));
    }
}

inline void set_uniform_4f(int id, float x, float y, float z, float w)
{
    LOG_CONTEXT("set_uniform_4f", error);
    if(id != -1) {
        GL_CHECK(glUniform4f(id, x, y, z, w));
    }
}

inline void set_uniform_4fv(int id, int n, float const *v)
{
    LOG_CONTEXT("set_uniform_4fv", error);
    if(id != -1) {
        GL_CHECK(glUniform4fv(id, n, v));
    }
}

namespace gl
{
    //////////////////////////////////////////////////////////////////////

    struct vertex_solid
    {
        float x, y;
    };

    //////////////////////////////////////////////////////////////////////

    struct vertex_color
    {
        float x, y;
        uint32_t color;
    };

    //////////////////////////////////////////////////////////////////////

    struct vertex_entity
    {
        float x, y;
        uint32_t entity_id;
    };

    //////////////////////////////////////////////////////////////////////

    struct vertex_textured
    {
        float x, y;
        float u, v;
    };

    //////////////////////////////////////////////////////////////////////
    // base gl_program - transform matrix is common to all

    struct program_base
    {
        LOG_CONTEXT("gl_program", debug);

        virtual ~program_base() = default;

        GLuint program_id{};
        GLuint vertex_shader_id{};
        GLuint fragment_shader_id{};

        GLuint u_transform{};

        GLuint vao{};

        program_base() = default;

        char const *program_name;    // just for debugging reference

        std::string vertex_shader_source;
        std::string fragment_shader_source;

        int get_uniform(char const *name);
        int get_attribute(char const *name);

        int compile_shader(GLuint shader_type, char const *source) const;
        int validate(GLuint param) const;
        void activate() const;
        virtual int init();
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct render_target
    {
        static GLuint constexpr max_num_slots = 16;

        GLuint fbo{};

        std::vector<GLuint> texture_ids{};
        GLuint num_slots{};

        int width{};
        int height{};
        int num_samples{};

        render_target() = default;

        int init(GLuint new_width, GLuint new_height, GLuint multisample_count, GLuint slots);
        void bind_framebuffer() const;
        void bind_textures() const;
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct index_buffer
    {
        GLuint ibo_id{ 0 };
        int num_indices{ 0 };

        index_buffer() = default;

        int init(size_t index_count);
        int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct vertex_array
    {
        virtual ~vertex_array() = default;

        GLuint vao_id{ 0 };
        GLuint vbo_id{ 0 };
        int num_verts{ 0 };

        vertex_array() = default;

        void alloc(size_t vert_count, size_t vertex_size);

        virtual void init(size_t vert_count);
        virtual void activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct vertex_array_solid : vertex_array
    {
        using vertex_type = vertex_solid;

        vertex_array_solid() = default;

        void  init(size_t vert_count) override;
        void activate() const override;
    };

    //////////////////////////////////////////////////////////////////////
    // special for the quad points to make instanced thick lines

    struct vertex_array_quad_points : vertex_array
    {
        using vertex_type = vertex_solid;

        vertex_array_quad_points() = default;

        void init(size_t vert_count) override;
        void activate() const override;
    };

    //////////////////////////////////////////////////////////////////////
    // special for the quad points to make instanced thick lines

    struct vertex_array_entity : vertex_array
    {
        using vertex_type = vertex_solid;

        vertex_array_entity() = default;

        void init(size_t vert_count) override;
        void activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct vertex_array_color : vertex_array
    {
        using vertex_type = vertex_color;

        vertex_array_color() = default;

        void init(size_t vert_count) override;
        void activate() const override;
    };

    //////////////////////////////////////////////////////////////////////
    // uniform color

    struct solid_program : program_base
    {
        static int constexpr position_location = 0;

        GLuint u_transform;
        GLuint u_color;

        int init() override;

        void set_color(color solid_color) const;
    };

    //////////////////////////////////////////////////////////////////////
    // for drawing layers

    struct layer_program : program_base
    {
        static int constexpr position_location = 0;
        static int constexpr entity_id_location = 1;

        GLuint u_transform;
        GLuint u_flags_sampler;
        GLuint u_draw_flags;

        GLuint u_red_flags;
        GLuint u_green_flags;
        GLuint u_blue_flags;

        GLuint u_value;

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////
    // color per vertex

    struct color_program : program_base
    {
        static int constexpr position_location = 0;
        static int constexpr vert_color_location = 1;

        GLuint u_transform;

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////
    // draw a layer

    struct blit_program : program_base
    {
        GLuint u_fill_color;
        GLuint u_other_color;
        GLuint u_cover_sampler;
        GLuint u_num_samples{};

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////
    // draw selections in a layer

    struct selection_program : program_base
    {
        GLuint u_red_color;
        GLuint u_green_color;
        GLuint u_blue_color;
        GLuint u_cover_sampler;
        GLuint u_num_samples{};

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////

    struct line_program : program_base
    {
        static constexpr int position_location = 0;
        static constexpr int pos_a_location = 1;    // Instanced
        static constexpr int pos_b_location = 2;    // Instanced

        GLuint u_transform;
        GLuint u_thickness;
        GLuint u_viewport_size;
        GLuint u_color;

        static const float quad[8];

        vertex_array_quad_points quad_points_array;

        void set_color(color solid_color) const;

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////

    struct line2_program : program_base
    {
        static constexpr int position_location = 0;

        GLuint u_transform;
        GLuint u_thickness;        // global line thickness
        GLuint u_viewport_size;    // needed for pixel thickness

        GLuint u_red_flag;
        GLuint u_green_flag;
        GLuint u_blue_flag;

        GLuint u_red_color;
        GLuint u_green_color;
        GLuint u_blue_color;

        GLuint u_lines_sampler;    // sampler for TBO of struct lines, fetch based on gl_InstanceId
        GLuint u_vert_sampler;     // sampler for the vertices
        GLuint u_flags_sampler;    // sampler for flags (1 byte per entity_id)

        static const float quad[8];

        vertex_array_quad_points quad_points_array;

        int init() override;

        struct line
        {
            uint32_t start_index;
            uint32_t end_index;
            uint32_t entity_id;
            uint32_t pad;    // Gah
        };
    };

    //////////////////////////////////////////////////////////////////////

    struct arc_program : program_base
    {
        static constexpr int position_location = 0;
        static constexpr int center_location = 1;         // Instanced
        static constexpr int radius_location = 2;         // Instanced
        static constexpr int start_angle_location = 3;    // Instanced
        static constexpr int sweep_location = 4;          // Instanced
        static constexpr int extent_min_location = 5;     // Instanced
        static constexpr int extent_max_location = 6;     // Instanced

        GLuint u_transform;
        GLuint u_thickness;
        GLuint u_viewport_size;
        GLuint u_color;

        static const float quad[8];

        vertex_array_quad_points quad_points_array;

        void set_color(color solid_color) const;

        int init() override;

        struct arc
        {
            gerber_lib::vec2f center;
            float radius;
            float start_angle;
            float sweep;
            gerber_lib::vec2f extent_min;
            gerber_lib::vec2f extent_max;
        };
    };

    //////////////////////////////////////////////////////////////////////

    template <int X, typename T> void update_buffer(T const &elems)
    {
        LOG_CONTEXT("update_buffer", debug);
        using V = T::value_type;
        auto total = sizeof(V) * elems.size();
        void *v;
        GL_CHECK(v = glMapBufferRange(X, 0, total, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        if(v != nullptr) {
            memcpy(v, elems.data(), total);
        }
        GL_CHECK(glUnmapBuffer(X));
    }

    //////////////////////////////////////////////////////////////////////

    struct texture
    {
        GLuint texture_id{};
        GLuint width{};
        GLuint height{};

        texture() = default;

        int init(GLuint w, GLuint h, uint32_t *data = nullptr);
        int bind() const;
        int update(uint32_t *data);
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct drawlist
    {
        using rect = gerber_lib::rect;
        using vec2d = gerber_lib::vec2d;

        struct drawlist_entry
        {
            GLenum draw_type;
            GLuint offset;
            GLuint count;
        };

        static int constexpr max_verts = 8192;

        vertex_array_color vertex_array;

        std::vector<vertex_color> verts;
        std::vector<drawlist_entry> drawlist_entries;

        void init()
        {
            vertex_array.init(max_verts);
            reset();
        }

        void reset()
        {
            verts.clear();
            verts.reserve(max_verts);
            drawlist_entries.clear();
        }

        void add_vertex(vec2d const &pos, color color)
        {
            if(drawlist_entries.empty()) {
                return;
            }
            if(verts.size() >= max_verts) {
                return;
            }
            verts.emplace_back(static_cast<float>(pos.x), static_cast<float>(pos.y), color);
            drawlist_entries.back().count += 1;
        }

        void add_drawlist_entry(GLenum type)
        {
            drawlist_entries.emplace_back(type, static_cast<GLuint>(verts.size()), 0);
        }

        void lines()
        {
            add_drawlist_entry(GL_LINES);
        }

        void add_line(vec2d const &start, vec2d const &end, color color)
        {
            add_vertex(start, color);
            add_vertex(end, color);
        }

        void add_outline_rect(rect const &r, uint32_t color)
        {
            add_drawlist_entry(GL_LINE_STRIP);
            add_vertex(r.min_pos, color);
            add_vertex({ r.max_pos.x, r.min_pos.y }, color);
            add_vertex(r.max_pos, color);
            add_vertex({ r.min_pos.x, r.max_pos.y }, color);
            add_vertex(r.min_pos, color);
        }

        void add_rect(rect const &r, uint32_t color)
        {
            add_drawlist_entry(GL_TRIANGLE_FAN);
            add_vertex(r.min_pos, color);
            add_vertex({ r.max_pos.x, r.min_pos.y }, color);
            add_vertex(r.max_pos, color);
            add_vertex({ r.min_pos.x, r.max_pos.y }, color);
        }

        void draw();
    };

}    // namespace gl
