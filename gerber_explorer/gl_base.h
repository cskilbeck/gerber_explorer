#pragma once

#include <cstdint>

#include "gerber_log.h"
#include "gerber_2d.h"

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

    struct gl_program
    {
        virtual ~gl_program() = default;
        LOG_CONTEXT("gl_program", debug);

        GLuint program_id{};
        GLuint vertex_shader_id{};
        GLuint fragment_shader_id{};

        GLuint transform_location{};

        gl_program() = default;

        char const *program_name;    // just for debugging reference

        char const *vertex_shader_source;
        char const *fragment_shader_source;

        int get_uniform(char const *name);
        int get_attribute(char const *name);

        int compile_shader(GLuint shader_type, char const *source) const;
        int validate(GLuint param) const;
        void use() const;
        virtual int init();
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_render_target
    {
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
    // uniform color

    struct gl_solid_program : gl_program
    {
        GLuint color_location{};

        int init() override;

        void set_color(uint32_t solid_color) const;
    };

    //////////////////////////////////////////////////////////////////////
    // for drawing layers

    struct gl_layer_program : gl_program
    {
        GLuint color_location{};

        GLuint center_uniform;
        GLuint x_flip_uniform;
        GLuint y_flip_uniform;

        int init() override;

        void set_color(uint32_t cover) const;
    };

    //////////////////////////////////////////////////////////////////////
    // color per vertex

    struct gl_color_program : gl_program
    {
        int init() override;
    };

    //////////////////////////////////////////////////////////////////////
    // textured, no color

    struct gl_textured_program : gl_program
    {
        GLuint red_color_uniform;
        GLuint green_color_uniform;
        GLuint blue_color_uniform;
        GLuint alpha_uniform;
        GLuint cover_sampler;
        GLuint num_samples_uniform{};

        int init() override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_index_array
    {
        GLuint ibo_id{ 0 };
        int num_indices{ 0 };

        gl_index_array() = default;

        int init(GLsizei index_count);
        int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array
    {
        virtual ~gl_vertex_array() = default;

        GLuint vbo_id{ 0 };
        int num_verts{ 0 };

        gl_vertex_array() = default;

        int alloc(GLsizei vert_count, size_t vertex_size);

        virtual int init(gl_program &program, GLsizei vert_count) = 0;
        virtual int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_solid : gl_vertex_array
    {
        gl_vertex_array_solid() = default;

        int position_location{ 0 };

        int init(gl_program &program, GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_color : gl_vertex_array
    {
        gl_vertex_array_color() = default;

        int position_location{ 0 };
        int color_location{ 0 };

        int init(gl_program &program, GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_textured : gl_vertex_array
    {
        gl_vertex_array_textured() = default;

        int position_location{ 0 };
        int tex_coord_location{ 0 };

        int init(gl_program &program, GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    template <int X, typename T> void update_buffer(T const &elems)
    {
        using V = typename T::value_type;
        auto total = sizeof(V) * elems.size();
        void *v = glMapBufferRange(X, 0, total, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        memcpy(v, elems.data(), total);
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
        using rect = gerber_lib::gerber_2d::rect;
        using vec2d = gerber_lib::gerber_2d::vec2d;

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

        int init(gl_color_program &program)
        {
            int err = vertex_array.init(program, max_verts);
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

        void add_vertex(vec2d const &pos, uint32_t color)
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

        void add_line(vec2d const &start, vec2d const &end, uint32_t color)
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

//////////////////////////////////////////////////////////////////////

#if defined(_DEBUG) || 1
#define GL_CHECK(x)                                                                                                                                            \
    do {                                                                                                                                                       \
        x;                                                                                                                                                     \
        GLenum __err = glGetError();                                                                                                                           \
        if(__err != 0) {                                                                                                                                       \
            LOG_ERROR("ERROR {} from {} at line {} of {}", __err, #x, __LINE__, __FILE__);                                                                     \
        }                                                                                                                                                      \
    } while(0)
#else
#define GL_CHECK(x)                                                                                                                                            \
    do {                                                                                                                                                       \
        x;                                                                                                                                                     \
    } while(0)
#endif
