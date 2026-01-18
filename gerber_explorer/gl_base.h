#pragma once

#include <cstdint>

#include "gerber_log.h"
#include "gerber_2d.h"
#include "gl_colors.h"

//////////////////////////////////////////////////////////////////////

extern int gl_log;

#if defined(_DEBUG) || 1
#define GL_CHECK(x)                                                                        \
    do {                                                                                   \
        if(gl_log != 0) {                                                                  \
            LOG_INFO("{}", #x);                                                            \
        }                                                                                  \
        x;                                                                                 \
        GLenum __err = glGetError();                                                       \
        if(__err != 0) {                                                                   \
            LOG_ERROR("ERROR {} from {} at line {} of {}", __err, #x, __LINE__, __FILE__); \
        }                                                                                  \
    } while(0)
#else
#define GL_CHECK(x) \
    do {            \
        x;          \
    } while(0)
#endif

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_solid
    {
        float x, y;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_color
    {
        float x, y;
        uint32_t color;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_textured
    {
        float x, y;
        float u, v;
    };

    //////////////////////////////////////////////////////////////////////
    // base gl_program - transform matrix is common to all

    struct gl_program_base
    {
        LOG_CONTEXT("gl_program", debug);

        virtual ~gl_program_base() = default;

        GLuint program_id{};
        GLuint vertex_shader_id{};
        GLuint fragment_shader_id{};

        GLuint u_transform{};

        GLuint vao{};

        gl_program_base() = default;

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

    struct gl_render_target
    {
        static GLuint constexpr max_num_slots = 16;

        GLuint fbo{};

        std::vector<GLuint> texture_ids{};
        GLuint num_slots{};

        int width{};
        int height{};
        int num_samples{};

        gl_render_target() = default;

        int init(GLuint new_width, GLuint new_height, GLuint multisample_count, GLuint slots);
        void bind_framebuffer() const;
        void bind_textures() const;
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_index_buffer
    {
        GLuint ibo_id{ 0 };
        int num_indices{ 0 };

        gl_index_buffer() = default;

        int init(GLsizei index_count);
        int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array
    {
        virtual ~gl_vertex_array() = default;

        GLuint vao_id{ 0 };
        GLuint vbo_id{ 0 };
        int num_verts{ 0 };

        gl_vertex_array() = default;

        int alloc(GLsizei vert_count, size_t vertex_size);

        virtual int init(GLsizei vert_count);
        virtual int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_solid : gl_vertex_array
    {
        gl_vertex_array_solid() = default;

        int init(GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////
    // special for the quad points to make instanced thick lines

    struct gl_vertex_array_quad_points : gl_vertex_array
    {
        gl_vertex_array_quad_points() = default;

        int init(GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_color : gl_vertex_array
    {
        gl_vertex_array_color() = default;

        int init(GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////
    // uniform color

    struct gl_solid_program : gl_program_base
    {
        static int constexpr position_location = 0;

        GLuint u_transform;
        GLuint u_color;

        int init() override;

        void set_color(gl::color solid_color) const;
    };

    //////////////////////////////////////////////////////////////////////
    // for drawing layers

    struct gl_layer_program : gl_program_base
    {
        static int constexpr position_location = 0;

        GLuint u_transform;
        GLuint u_color{};

        int init() override;

        void set_color(gl::color cover) const;
    };

    //////////////////////////////////////////////////////////////////////
    // color per vertex

    struct gl_color_program : gl_program_base
    {
        static int constexpr position_location = 0;
        static int constexpr vert_color_location = 1;

        GLuint u_transform;

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////
    // textured, no color

    struct gl_textured_program : gl_program_base
    {
        GLuint u_red;
        GLuint u_green;
        GLuint u_blue;
        GLuint u_alpha;
        GLuint u_cover_sampler;
        GLuint u_num_samples{};

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_line_program : gl_program_base
    {
        static constexpr int position_location = 0;
        static constexpr int pos_a_location = 1;    // Instanced
        static constexpr int pos_b_location = 2;    // Instanced

        GLuint u_transform;
        GLuint u_thickness;
        GLuint u_viewport_size;
        GLuint u_color;

        static const float quad[8];

        gl_vertex_array_quad_points quad_points_array;

        void set_color(gl::color solid_color) const;

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_line2_program : gl_program_base
    {
        static constexpr int position_location = 0;

        GLuint u_transform;
        GLuint u_thickness;        // global line thickness
        GLuint u_viewport_size;    // needed for pixel thickness
        GLuint u_hover_color;      // coverage colors for hovered
        GLuint u_select_color;     // and selected lines

        GLuint u_lines_sampler;    // sampler for TBO of struct lines, fetch based on gl_InstanceId
        GLuint u_vert_sampler;     // sampler for the vertices
        GLuint u_flags_sampler;    // sampler for flags (1 byte per entity_id)

        static const float quad[8];

        gl_vertex_array_quad_points quad_points_array;

        void set_hover_color(gl::color hover_color) const;
        void set_select_color(gl::color select_color) const;

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

    struct gl_arc_program : gl_program_base
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

        gl_vertex_array_quad_points quad_points_array;

        void set_color(gl::color solid_color) const;

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
        using V = typename T::value_type;
        auto total = sizeof(V) * elems.size();
        void *v;
        GL_CHECK(v = glMapBufferRange(X, 0, total, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        if(v != nullptr) {
            memcpy(v, elems.data(), total);
        }
        glUnmapBuffer(X);
    }

    //////////////////////////////////////////////////////////////////////

    struct gl_texture
    {
        GLuint texture_id{};
        GLuint width{};
        GLuint height{};

        gl_texture() = default;

        int init(GLuint w, GLuint h, uint32_t *data = nullptr);
        int bind() const;
        int update(uint32_t *data);
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_drawlist
    {
        using rect = gerber_lib::rect;
        using vec2d = gerber_lib::vec2d;

        struct gl_drawlist_entry
        {
            GLenum draw_type;
            GLuint offset;
            GLuint count;
        };

        static int constexpr max_verts = 8192;

        gl_vertex_array_color vertex_array;

        std::vector<gl_vertex_color> verts;
        std::vector<gl_drawlist_entry> drawlist;

        int init()
        {
            int err = vertex_array.init(max_verts);
            if(err != 0) {
                return err;
            }
            reset();
            return 0;
        }

        void reset()
        {
            verts.clear();
            verts.reserve(max_verts);
            drawlist.clear();
        }

        void add_vertex(vec2d const &pos, gl::color color)
        {
            if(drawlist.empty()) {
                return;
            }
            if(verts.size() >= max_verts) {
                return;
            }
            verts.emplace_back(static_cast<float>(pos.x), static_cast<float>(pos.y), color);
            drawlist.back().count += 1;
        }

        void add_drawlist_entry(GLenum type)
        {
            drawlist.emplace_back(type, static_cast<GLuint>(verts.size()), 0);
        }

        void lines()
        {
            add_drawlist_entry(GL_LINES);
        }

        void add_line(vec2d const &start, vec2d const &end, gl::color color)
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

}    // namespace gerber_3d
