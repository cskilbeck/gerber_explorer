//////////////////////////////////////////////////////////////////////

#include <cstring>

#include <cmrc/cmrc.hpp>

#include "gerber_log.h"
#include "gerber_lib.h"
#include "gerber_util.h"

#include "glad/glad.h"

#include "gl_base.h"
#include "gl_colors.h"

#include <array>

LOG_CONTEXT("gl_base", debug);

CMRC_DECLARE(my_shaders);

int gl_log = 0;

namespace
{
    std::string shader_src(char const *name)
    {
        auto my_shaders = cmrc::my_shaders::get_filesystem();
        if(!my_shaders.is_file(name)) {
            return {};
        }
        auto src = my_shaders.open(name);
        return std::string(src.begin(), src.size());
    }

    //////////////////////////////////////////////////////////////////////
    // quads for instanced line/arc shaders

    const std::array<gl::vertex_solid, 4> line_quad_verts{ -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
    const std::array<gl::vertex_solid, 4> arc_quad_verts{ 0, 0, 1, 0, 0, 1, 1, 1 };

}    // namespace

namespace gl
{
    //////////////////////////////////////////////////////////////////////

    char const *shader_version_preamble = "#version 410 core\n";

#if defined(_DEBUG)
    char const *shader_debug_preamble = R"PREAMBLE(
#define DEBUG
#define _DEBUG
    )PREAMBLE";
#else
    char const *shader_debug_preamble = R"PREAMBLE(
#define NDEBUG
    )PREAMBLE";
#endif

    int program_base::compile_shader(GLenum shader_type, char const *source) const
    {
        while(glGetError() != GL_NO_ERROR) {}

        char const *sources[] = { shader_version_preamble, shader_debug_preamble, source };
        GLuint id;
        GL_CHECK(id = glCreateShader(shader_type));
        GL_CHECK(glShaderSource(id, std::size(sources), sources, NULL));
        GL_CHECK(glCompileShader(id));

        int result;
        GL_CHECK(glGetShaderiv(id, GL_COMPILE_STATUS, &result));
        if(result) {
            return id;
        }
        GLsizei length;
        GL_CHECK(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
        if(length != 0) {
            GLchar *info_log = new GLchar[length];
            GL_CHECK(glGetShaderInfoLog(id, length, &length, info_log));
            LOG_ERROR("Error in shader \"{}\": {}", program_name, info_log);
            delete[] info_log;
        } else {
            LOG_ERROR("Huh? Compile error but no log?");
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int program_base::validate(GLuint param) const
    {
        GLint result;
        GL_CHECK(glGetProgramiv(program_id, param, &result));
        if(result) {
            return 0;
        }
        GLsizei length;
        GL_CHECK(glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length));
        if(length != 0) {
            GLchar *info_log = new GLchar[length];
            GL_CHECK(glGetProgramInfoLog(program_id, length, &length, info_log));
            LOG_ERROR("Error in program \"{}\": {}", program_name, info_log);
            delete[] info_log;
        } else if(param == GL_LINK_STATUS) {
            LOG_ERROR("glLinkProgram failed: Can not link program.");
        } else {
            LOG_ERROR("glValidateProgram failed: Can not execute shader program.");
        }
        return -1;
    }

    //////////////////////////////////////////////////////////////////////
    // base without transform

    int program_base::init()
    {
        if(vertex_shader_source.empty() || fragment_shader_source.empty()) {
            LOG_ERROR("SHADER SOURCE MISSING");
            return 1;
        }
        vertex_shader_id = compile_shader(GL_VERTEX_SHADER, vertex_shader_source.c_str());
        fragment_shader_id = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source.c_str());

        GL_CHECK(program_id = glCreateProgram());
        GL_CHECK(glAttachShader(program_id, vertex_shader_id));
        GL_CHECK(glAttachShader(program_id, fragment_shader_id));

        GL_CHECK(glLinkProgram(program_id));
        int rc = validate(GL_LINK_STATUS);
        if(rc != 0) {
            cleanup();
            LOG_ERROR("validate error for init() {}", program_name);
            return rc;
        }
        activate();
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int program_base::get_uniform(char const *name)
    {
        int location = glGetUniformLocation(program_id, name);
        if(location == (int)GL_INVALID_INDEX) {
            LOG_WARNING("Can't get uniform location for \"{}\" in program \"{}\"", name, program_name);
        }
        return location;
    }

    //////////////////////////////////////////////////////////////////////

    int program_base::get_attribute(char const *name)
    {
        int location = glGetAttribLocation(program_id, name);
        if(location == (int)GL_INVALID_INDEX) {
            LOG_ERROR("Can't get attribute location for \"{}\" in program \"{}\"", name, program_name);
        }
        return location;
    }

    //////////////////////////////////////////////////////////////////////

    void program_base::activate() const
    {
        GL_CHECK(glUseProgram(program_id));
    }

    //////////////////////////////////////////////////////////////////////

    void line_program::set_color(color solid_color) const
    {
        colorf4 f(solid_color);
        set_uniform_4f(u_color, f.red(), f.green(), f.blue(), f.alpha());
    }

    //////////////////////////////////////////////////////////////////////

    int line_program::init()
    {
        program_name = "line";

        vertex_shader_source = shader_src("line_vertex_shader.glsl");
        fragment_shader_source = shader_src("line_fragment_shader.glsl");

        int err = program_base::init();
        if(err != 0) {
            return err;
        }
        quad_points_array.init(4);
        quad_points_array.activate();
        update_buffer<GL_ARRAY_BUFFER>(line_quad_verts);

        // attribute for instanced line start
        for(int i = pos_a_location; i <= pos_b_location; ++i) {
            GL_CHECK(glEnableVertexAttribArray(i));
            GL_CHECK(glVertexAttribDivisor(i, 1));
        }

        // attribute for instanced quad points already enabled in line_array.activate()
        // GL_CHECK(glEnableVertexAttribArray(2));
        // GL_CHECK(glVertexAttribDivisor(2, 0));

        u_transform = get_uniform("transform");
        u_thickness = get_uniform("thickness");
        u_viewport_size = get_uniform("viewport_size");
        u_color = get_uniform("color");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int line2_program::init()
    {
        program_name = "line";

        vertex_shader_source = shader_src("line2_vertex_shader.glsl");
        fragment_shader_source = shader_src("line2_fragment_shader.glsl");

        int err = program_base::init();
        if(err != 0) {
            return err;
        }
        quad_points_array.init(4);
        quad_points_array.activate();
        update_buffer<GL_ARRAY_BUFFER>(line_quad_verts);

        u_transform = get_uniform("transform");
        u_thickness = get_uniform("thickness");
        u_viewport_size = get_uniform("viewport_size");
        u_red_flag = get_uniform("red_flag");
        u_green_flag = get_uniform("green_flag");
        u_blue_flag = get_uniform("blue_flag");
        u_red_color = get_uniform("red_color");
        u_green_color = get_uniform("green_color");
        u_blue_color = get_uniform("blue_color");
        u_lines_sampler = get_uniform("instance_sampler");
        u_vert_sampler = get_uniform("vert_sampler");
        u_flags_sampler = get_uniform("flags_sampler");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void arc_program::set_color(color solid_color) const
    {
        colorf4 f(solid_color);
        set_uniform_4f(u_color, f.red(), f.green(), f.blue(), f.alpha());
    }

    //////////////////////////////////////////////////////////////////////

    int arc_program::init()
    {
        program_name = "arc";

        vertex_shader_source = shader_src("arc_vertex_shader.glsl");
        fragment_shader_source = shader_src("arc_fragment_shader.glsl");

        int err = program_base::init();
        if(err != 0) {
            return err;
        }
        quad_points_array.init(4);
        quad_points_array.activate();
        update_buffer<GL_ARRAY_BUFFER>(arc_quad_verts);

        // enable instanced elements
        for(int i = center_location; i <= extent_max_location; ++i) {
            GL_CHECK(glEnableVertexAttribArray(i));
            GL_CHECK(glVertexAttribDivisor(i, 1));
        }
        u_transform = get_uniform("transform");
        u_thickness = get_uniform("thickness");
        u_viewport_size = get_uniform("viewport_size");
        u_color = get_uniform("color");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int layer_program::init()
    {
        program_name = "layer";

        vertex_shader_source = shader_src("layer_vertex_shader.glsl");
        fragment_shader_source = shader_src("layer_fragment_shader.glsl");

        int err = program_base::init();
        if(err != 0) {
            return err;
        }

        u_transform = get_uniform("transform");
        u_flags_sampler = get_uniform("flags_sampler");
        u_draw_flags = get_uniform("draw_flags");

        u_red_flags = get_uniform("red_flags");
        u_green_flags = get_uniform("green_flags");
        u_blue_flags = get_uniform("blue_flags");

        u_value = get_uniform("value");

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int solid_program::init()
    {
        program_name = "solid";
        vertex_shader_source = shader_src("solid_vertex_shader.glsl");
        fragment_shader_source = shader_src("common_fragment_shader.glsl");
        int err = program_base::init();
        if(err != 0) {
            return err;
        }
        u_transform = get_uniform("transform");
        u_color = get_uniform("uniform_color");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void solid_program::set_color(color solid_color) const
    {
        colorf4 f(solid_color);
        set_uniform_4f(u_color, f.red(), f.green(), f.blue(), f.alpha());
    }

    //////////////////////////////////////////////////////////////////////

    int color_program::init()
    {
        program_name = "color";
        vertex_shader_source = shader_src("color_vertex_shader.glsl");
        fragment_shader_source = shader_src("common_fragment_shader.glsl");
        int err = program_base::init();
        if(err != 0) {
            return err;
        }
        u_transform = get_uniform("transform");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int blit_program::init()
    {
        program_name = "blit";
        vertex_shader_source = shader_src("blit_vertex_shader.glsl");
        fragment_shader_source = shader_src("blit_fragment_shader.glsl");
        int err = program_base::init();
        if(err != 0) {
            return err;
        }
        u_fill_color = get_uniform("fill_color");
        u_other_color = get_uniform("other_color");
        u_cover_sampler = get_uniform("cover_sampler");
        u_num_samples = get_uniform("num_samples");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int selection_program::init()
    {
        program_name = "selection";
        vertex_shader_source = shader_src("blit_vertex_shader.glsl");
        fragment_shader_source = shader_src("selection_fragment_shader.glsl");
        int err = program_base::init();
        if(err != 0) {
            return err;
        }
        u_red_color = get_uniform("red_color");
        u_green_color = get_uniform("green_color");
        u_blue_color = get_uniform("blue_color");
        u_cover_sampler = get_uniform("cover_sampler");
        u_num_samples = get_uniform("num_samples");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void program_base::cleanup()
    {
        GL_CHECK(glDetachShader(program_id, vertex_shader_id));
        GL_CHECK(glDetachShader(program_id, fragment_shader_id));

        GL_CHECK(glDeleteShader(vertex_shader_id));
        vertex_shader_id = 0;

        GL_CHECK(glDeleteShader(fragment_shader_id));
        fragment_shader_id = 0;

        GL_CHECK(glDeleteProgram(program_id));
        program_id = 0;
    }

    //////////////////////////////////////////////////////////////////////

    int index_buffer::init(size_t index_count)
    {
        GL_CHECK(glGenBuffers(1, &ibo_id));
        num_indices = (int)index_count;
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
        GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * num_indices, nullptr, GL_DYNAMIC_DRAW));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void index_buffer::activate() const
    {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
    }

    //////////////////////////////////////////////////////////////////////

    void index_buffer::cleanup()
    {
        if(ibo_id != 0) {
            GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            GLuint buffers[] = { ibo_id };
            GL_CHECK(glDeleteBuffers(static_cast<GLsizei>(gerber_util::array_length(buffers)), buffers));

            ibo_id = 0;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array::init(size_t vert_count)
    {
        GL_CHECK(glGenVertexArrays(1, &vao_id));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array::alloc(size_t vert_count, size_t vertex_size)
    {
        GL_CHECK(glGenBuffers(1, &vbo_id));

        num_verts = (GLsizei)vert_count;

        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertex_size * num_verts, nullptr, GL_DYNAMIC_DRAW));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_solid::init(size_t vert_count)
    {
        vertex_array::init(vert_count);
        alloc(vert_count, sizeof(vertex_solid));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_color::init(size_t vert_count)
    {
        vertex_array::init(vert_count);
        alloc(vert_count, sizeof(vertex_color));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array::activate() const
    {
        GL_CHECK(glBindVertexArray(vao_id));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_solid::activate() const
    {
        vertex_array::activate();
        GL_CHECK(glEnableVertexAttribArray(solid_program::position_location));
        GL_CHECK(glVertexAttribPointer(solid_program::position_location, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_solid), (void *)(offsetof(vertex_solid, x))));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_entity::init(size_t vert_count)
    {
        vertex_array::init(vert_count);
        alloc(vert_count, sizeof(vertex_entity));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_entity::activate() const
    {
        vertex_array::activate();
        GL_CHECK(glEnableVertexAttribArray(layer_program::position_location));
        GL_CHECK(glVertexAttribPointer(layer_program::position_location, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_entity), (void *)(offsetof(vertex_entity, x))));
        GL_CHECK(glEnableVertexAttribArray(layer_program::entity_id_location));
        GL_CHECK(glVertexAttribIPointer(
            layer_program::entity_id_location, 1, GL_UNSIGNED_INT, sizeof(vertex_entity), (void *)(offsetof(vertex_entity, entity_id))));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_quad_points::init(size_t vert_count)
    {
        // this is for the quad verts
        vertex_array::init(vert_count);
        alloc(vert_count, sizeof(vertex_solid));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_quad_points::activate() const
    {
        vertex_array::activate();
        GL_CHECK(glEnableVertexAttribArray(line_program::position_location));
        GL_CHECK(glVertexAttribPointer(line_program::position_location, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_solid), (void *)(offsetof(vertex_solid, x))));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array_color::activate() const
    {
        vertex_array::activate();
        GL_CHECK(glEnableVertexAttribArray(color_program::position_location));
        GL_CHECK(glEnableVertexAttribArray(color_program::vert_color_location));
        GL_CHECK(glVertexAttribPointer(color_program::position_location, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_color), (void *)(offsetof(vertex_color, x))));
        GL_CHECK(glVertexAttribPointer(color_program::vert_color_location,
                              4,
                              GL_UNSIGNED_BYTE,
                              GL_TRUE,
                              sizeof(vertex_color),
                              reinterpret_cast<void *>(offsetof(vertex_color, color))));
    }

    //////////////////////////////////////////////////////////////////////

    void vertex_array::cleanup()
    {
        if(vbo_id != 0) {
            GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
            GL_CHECK(glDeleteBuffers(1, &vbo_id));
            vbo_id = 0;
        }
        if(vao_id != 0) {
            GL_CHECK(glDeleteVertexArrays(1, &vao_id));
            vao_id = 0;
        }
        num_verts = 0;
    }

    //////////////////////////////////////////////////////////////////////

    int texture::init(GLuint w, GLuint h, uint32_t const *data)
    {
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
        GL_CHECK(glGenTextures(1, &texture_id));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture_id));
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        width = w;
        height = h;
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int texture::bind() const
    {
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int texture::update([[maybe_unused]] uint32_t *data)
    {
        LOG_ERROR("NOPE");
        return 1;
    }

    //////////////////////////////////////////////////////////////////////

    void texture::cleanup()
    {
        if(texture_id != 0) {
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
            GLuint t[] = { texture_id };
            GL_CHECK(glDeleteTextures(1, t));
            texture_id = 0;
        }
    }

    //////////////////////////////////////////////////////////////////////

    int render_target::init(GLuint new_width, GLuint new_height, GLuint multisample_count, GLuint slots)
    {
        if(num_slots > max_num_slots) {
            cleanup();
            return 1;
        }

        num_samples = multisample_count;
        num_slots = slots;

        if(new_width != 0 && new_height != 0) {

            GL_CHECK(glGenFramebuffers(1, &fbo));
            GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

            GLuint slot = 0;
            for(GLuint i = 0; i < num_slots; ++i) {
                GLuint texture_id;
                GL_CHECK(glGenTextures(1, &texture_id));
                GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture_id));
                GL_CHECK(glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample_count, GL_RGBA, new_width, new_height, GL_FALSE));
                GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_TEXTURE_2D_MULTISAMPLE, texture_id, 0));
                texture_ids.push_back(texture_id);
                slot += 1;
            }

            GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if(err != GL_FRAMEBUFFER_COMPLETE) {
                LOG_ERROR("glCheckFramebufferStatus failed: {:04x}", err);
                cleanup();
                return 1;
            }
            width = new_width;
            height = new_height;

            GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void render_target::bind_framebuffer() const
    {
        bind_textures();
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
        for(GLuint slot = 0; slot < num_slots; ++slot) {
            GL_CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_TEXTURE_2D_MULTISAMPLE, texture_ids[slot], 0));
        }
        std::array<GLenum, max_num_slots> buffers;
        GLenum a = GL_COLOR_ATTACHMENT0;
        for(GLuint i = 0; i < num_slots; ++i) {
            buffers[i] = a;
            a += 1;
        }
        GL_CHECK(glDrawBuffers(static_cast<GLsizei>(num_slots), buffers.data()));
    }

    //////////////////////////////////////////////////////////////////////

    void render_target::bind_textures() const
    {
        for(GLuint slot = 0; slot < num_slots; ++slot) {
            if(texture_ids[slot] != 0) {
                GL_CHECK(glActiveTexture(GL_TEXTURE0 + slot));
                GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture_ids[slot]));
            } else {
                LOG_ERROR("Can't bind!?");
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void render_target::cleanup()
    {
        if(num_slots == 0) {
            return;
        }

        GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

        if(!texture_ids.empty()) {
            GLuint *t = texture_ids.data();
            GL_CHECK(glDeleteTextures(num_slots, t));
            texture_ids.clear();
        }

        if(fbo != 0) {
            GLuint f[] = { fbo };
            GL_CHECK(glDeleteFramebuffers(1, f));
            fbo = 0;
        }
        num_slots = 0;
    }

    //////////////////////////////////////////////////////////////////////

    void drawlist::draw() const
    {
        if(!drawlist_entries.empty()) {
            vertex_array.activate();
            vertex_color *v;
            GL_CHECK(v = static_cast<vertex_color *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY)));
            memcpy(v, verts.data(), verts.size() * sizeof(vertex_color));
            GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));
            GL_CHECK(glEnable(GL_BLEND));
            GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            for(auto const &d : drawlist_entries) {
                GL_CHECK(glDrawArrays(d.draw_type, d.offset, d.count));
            }
        }
    }
}    // namespace gerber_3d
