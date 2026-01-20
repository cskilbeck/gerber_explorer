//////////////////////////////////////////////////////////////////////

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
    // a quad for instanced line/arc shaders

    const std::array<gerber_3d::gl_vertex_solid, 4> line_quad_verts{ -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
    const std::array<gerber_3d::gl_vertex_solid, 4> arc_quad_verts{ 0, 0, 1, 0, 0, 1, 1, 1 };

}    // namespace

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    int gl_program_base::compile_shader(GLenum shader_type, char const *source) const
    {
        GLuint id;
        GL_CHECK(id = glCreateShader(shader_type));
        GL_CHECK(glShaderSource(id, 1, &source, NULL));
        GL_CHECK(glCompileShader(id));

        GLint result;
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

    int gl_program_base::validate(GLuint param) const
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

    int gl_program_base::init()
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
        GL_CHECK(glValidateProgram(program_id));
        rc = validate(GL_VALIDATE_STATUS);
        if(rc != 0) {
            cleanup();
            LOG_ERROR("glValidateProgram error for {}", program_name);
            return rc;
        }
        activate();
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_program_base::get_uniform(char const *name)
    {
        int location;
        GL_CHECK(location = glGetUniformLocation(program_id, name));
        if(location == GL_INVALID_INDEX) {
            LOG_WARNING("Can't get uniform location for \"{}\" in program \"{}\"", name, program_name);
        }
        return location;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_program_base::get_attribute(char const *name)
    {
        int location;
        GL_CHECK(location = glGetAttribLocation(program_id, name));
        if(location == GL_INVALID_INDEX) {
            LOG_ERROR("Can't get attribute location for \"{}\" in program \"{}\"", name, program_name);
        }
        return location;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program_base::activate() const
    {
        GL_CHECK(glUseProgram(program_id));
    }

    //////////////////////////////////////////////////////////////////////

    void gl_line_program::set_color(gl::color solid_color) const
    {
        gl::colorf4 f(solid_color);
        GL_CHECK(glUniform4f(u_color, f.red(), f.green(), f.blue(), f.alpha()));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_line_program::init()
    {
        program_name = "line";

        vertex_shader_source = shader_src("line_vertex_shader.glsl");
        fragment_shader_source = shader_src("line_fragment_shader.glsl");

        int err = gl_program_base::init();
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

    int gl_line2_program::init()
    {
        program_name = "line";

        vertex_shader_source = shader_src("line2_vertex_shader.glsl");
        fragment_shader_source = shader_src("line2_fragment_shader.glsl");

        int err = gl_program_base::init();
        if(err != 0) {
            return err;
        }
        quad_points_array.init(4);
        quad_points_array.activate();
        update_buffer<GL_ARRAY_BUFFER>(line_quad_verts);

        u_transform = get_uniform("transform");
        u_thickness = get_uniform("thickness");
        u_viewport_size = get_uniform("viewport_size");
        u_hover_color = get_uniform("hover_color");
        u_select_color = get_uniform("select_color");
        u_lines_sampler = get_uniform("instance_sampler");
        u_vert_sampler = get_uniform("vert_sampler");
        u_flags_sampler = get_uniform("flags_sampler");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_arc_program::set_color(gl::color solid_color) const
    {
        gl::colorf4 f(solid_color);
        GL_CHECK(glUniform4f(u_color, f.red(), f.green(), f.blue(), f.alpha()));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_arc_program::init()
    {
        program_name = "arc";

        vertex_shader_source = shader_src("arc_vertex_shader.glsl");
        fragment_shader_source = shader_src("arc_fragment_shader.glsl");

        int err = gl_program_base::init();
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

    int gl_layer_program::init()
    {
        program_name = "layer";

        vertex_shader_source = shader_src("layer_vertex_shader.glsl");
        fragment_shader_source = shader_src("layer_fragment_shader.glsl");

        int err = gl_program_base::init();
        if(err != 0) {
            return err;
        }

        u_transform = get_uniform("transform");
        u_color = get_uniform("u_color");

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_layer_program::set_color(gl::color cover) const
    {
        gl::colorf4 f(cover);
        GL_CHECK(glUniform4f(u_color, f.red(), f.green(), f.blue(), f.alpha()));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_solid_program::init()
    {
        program_name = "solid";
        vertex_shader_source = shader_src("solid_vertex_shader.glsl");
        fragment_shader_source = shader_src("common_fragment_shader.glsl");
        int err = gl_program_base::init();
        if(err != 0) {
            return err;
        }
        u_transform = get_uniform("transform");
        u_color = get_uniform("uniform_color");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_solid_program::set_color(gl::color solid_color) const
    {
        gl::colorf4 f(solid_color);
        GL_CHECK(glUniform4f(u_color, f.red(), f.green(), f.blue(), f.alpha()));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_color_program::init()
    {
        program_name = "color";
        vertex_shader_source = shader_src("color_vertex_shader.glsl");
        fragment_shader_source = shader_src("common_fragment_shader.glsl");
        int err = gl_program_base::init();
        if(err != 0) {
            return err;
        }
        u_transform = get_uniform("transform");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_textured_program::init()
    {
        program_name = "textured";
        vertex_shader_source = shader_src("textured_vertex_shader.glsl");
        fragment_shader_source = shader_src("textured_fragment_shader.glsl");
        int err = gl_program_base::init();
        if(err != 0) {
            return err;
        }
        u_red = get_uniform("red_color");
        u_green = get_uniform("green_color");
        u_blue = get_uniform("blue_color");
        u_alpha = get_uniform("alpha");
        u_cover_sampler = get_uniform("cover_sampler");
        u_num_samples = get_uniform("num_samples");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program_base::cleanup()
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

    int gl_index_buffer::init(GLsizei index_count)
    {
        GL_CHECK(glGenBuffers(1, &ibo_id));
        num_indices = index_count;
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
        GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * num_indices, nullptr, GL_DYNAMIC_DRAW));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_index_buffer::activate() const
    {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_index_buffer::cleanup()
    {
        if(ibo_id != 0) {
            GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            GLuint buffers[] = { ibo_id };
            GL_CHECK(glDeleteBuffers(static_cast<GLsizei>(gerber_util::array_length(buffers)), buffers));

            ibo_id = 0;
        }
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::init(GLsizei vert_count)
    {
        GL_CHECK(glGenVertexArrays(1, &vao_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::alloc(GLsizei vert_count, size_t vertex_size)
    {
        GL_CHECK(glGenBuffers(1, &vbo_id));

        num_verts = vert_count;

        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertex_size * num_verts, nullptr, GL_DYNAMIC_DRAW));

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::init(GLsizei vert_count)
    {
        gl_vertex_array::init(vert_count);
        int err = alloc(vert_count, sizeof(gl_vertex_solid));
        if(err != 0) {
            return err;
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::init(GLsizei vert_count)
    {
        gl_vertex_array::init(vert_count);
        int err = alloc(vert_count, sizeof(gl_vertex_color));
        if(err != 0) {
            return err;
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::activate() const
    {
        GL_CHECK(glBindVertexArray(vao_id));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(gl_solid_program::position_location);
        glVertexAttribPointer(gl_solid_program::position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_solid), (void *)(offsetof(gl_vertex_solid, x)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_quad_points::init(GLsizei vert_count)
    {
        // this is for the quad verts
        gl_vertex_array::init(vert_count);
        int err = alloc(vert_count, sizeof(gl_vertex_solid));
        if(err != 0) {
            return err;
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_quad_points::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(gl_line_program::position_location);
        glVertexAttribPointer(gl_line_program::position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_solid), (void *)(offsetof(gl_vertex_solid, x)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(gl_color_program::position_location);
        glEnableVertexAttribArray(gl_color_program::vert_color_location);
        glVertexAttribPointer(gl_color_program::position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_color), (void *)(offsetof(gl_vertex_color, x)));
        glVertexAttribPointer(gl_color_program::vert_color_location,
                              4,
                              GL_UNSIGNED_BYTE,
                              GL_TRUE,
                              sizeof(gl_vertex_color),
                              reinterpret_cast<void *>(offsetof(gl_vertex_color, color)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_vertex_array::cleanup()
    {
        if(vbo_id != 0) {
            GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

            GLuint buffers[] = { vbo_id };
            GL_CHECK(glDeleteBuffers(static_cast<GLsizei>(gerber_util::array_length(buffers)), buffers));

            vbo_id = 0;
        }
    }

    //////////////////////////////////////////////////////////////////////

    int gl_texture::init(GLuint w, GLuint h, uint32_t *data)
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

    int gl_texture::bind() const
    {
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_texture::update([[maybe_unused]] uint32_t *data)
    {
        LOG_ERROR("NOPE");
        return 1;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_texture::cleanup()
    {
        if(texture_id != 0) {
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
            GLuint t[] = { texture_id };
            GL_CHECK(glDeleteTextures(1, t));
            texture_id = 0;
        }
    }

    //////////////////////////////////////////////////////////////////////

    int gl_render_target::init(GLuint new_width, GLuint new_height, GLuint multisample_count, GLuint slots)
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

    void gl_render_target::bind_framebuffer() const
    {
        bind_textures();
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
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

    void gl_render_target::bind_textures() const
    {
        for(GLuint slot = 0; slot < num_slots; ++slot) {
            GL_CHECK(glActiveTexture(GL_TEXTURE0 + slot));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture_ids[slot]));
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_render_target::cleanup()
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

    void gl_drawlist::draw()
    {
        if(!drawlist.empty()) {
            vertex_array.activate();
            gl_vertex_color *v;
            GL_CHECK(v = static_cast<gl_vertex_color *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY)));
            memcpy(v, verts.data(), verts.size() * sizeof(gl_vertex_color));
            GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));
            GL_CHECK(glEnable(GL_BLEND));
            GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            for(auto const &d : drawlist) {
                GL_CHECK(glDrawArrays(d.draw_type, d.offset, d.count));
            }
        }
    }
}    // namespace gerber_3d
