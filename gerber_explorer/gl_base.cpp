//////////////////////////////////////////////////////////////////////

#include "gerber_log.h"
#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_util.h"

#include "glad/glad.h"

#include "gl_base.h"
#include "gl_colors.h"

LOG_CONTEXT("gl_base", debug);

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////
    // SOLID VERTEX

    char const *solid_vertex_shader_source = R"!(

        #version 400

        in vec2 position;

        out vec4 color;

        uniform mat4 transform;
        uniform vec4 uniform_color;

        void main() {
            gl_Position = transform * vec4(position, 0.0f, 1.0f);
            color = uniform_color;
        }

    )!";

    //////////////////////////////////////////////////////////////////////
    // COLOR-PER-VERT VERTEX

    char const *color_vertex_shader_source = R"!(

        #version 400

        in vec2 position;
        in vec4 vert_color_in;

        out vec4 color;

        uniform mat4 transform;

        void main() {
            gl_Position = transform * vec4(position, 0.0f, 1.0f);
            color = vert_color_in;
        }

    )!";

    //////////////////////////////////////////////////////////////////////
    // SOLID / COLOR FRAGMENT

    char const *common_fragment_shader_source = R"!(

        #version 400

        in vec4 color;

        out vec4 fragment;

        void main() {
            fragment = color;
        }

    )!";

    //////////////////////////////////////////////////////////////////////
    // LAYER VERTEX

    char const *layer_vertex_shader_source = R"!(

        #version 400

        uniform mat4 transform;
        uniform vec4 cover_in;

        in vec2 position;

        out vec4 cover;

        void main() {
            gl_Position = transform * vec4(position, 0.0f, 1.0f);
            cover = cover_in;
        }

    )!";

    //////////////////////////////////////////////////////////////////////
    // LAYER FRAGMENT

    char const *layer_fragment_shader_source = R"!(

        #version 400

        in vec4 cover;

        out vec4 fragment;

        void main() {
            fragment = cover;
        }

    )!";

    //////////////////////////////////////////////////////////////////////
    // FULLSCREEN VERTEX

    char const *textured_vertex_shader_source = R"!(

        #version 400

        in vec2 position;
        in vec2 tex_coord_in;

        out vec2 tex_coord;

        uniform mat4 transform;

        void main() {
            gl_Position = transform * vec4(position.xy, 0.0f, 1);
            tex_coord = tex_coord_in;
        }

    )!";

    //////////////////////////////////////////////////////////////////////
    // FULLSCREEN FRAGMENT

    char const *textured_fragment_shader_source = R"!(

        #version 400

        in vec2 tex_coord;
        
        out vec4 fragment;

        uniform vec4 red_color;
        uniform vec4 green_color;
        uniform vec4 blue_color;

        uniform float alpha;
        uniform int num_samples;

        uniform sampler2DMS cover_sampler;

        vec4 multisample(sampler2DMS sampler, ivec2 coord)
        {
            vec4 color = vec4(0,0,0,0);
            for (int i = 0; i < num_samples; i++) {
                color += texelFetch(sampler, coord, i);
            }
            return color / float(num_samples);
        }

        void main() {

            ivec2 tex_size = textureSize(cover_sampler);
            ivec2 uv = ivec2(tex_coord * tex_size);
            vec4 cover = multisample(cover_sampler, uv);
            vec4 r = vec4(cover.xxxx * red_color);
            vec4 g = vec4(cover.yyyy * green_color);
            vec4 b = vec4(cover.zzzz * blue_color);
            vec4 rgb = r + g + b;
            rgb.w *= alpha;
            fragment = rgb;
        }

    )!";

    //////////////////////////////////////////////////////////////////////

    int gl_program::compile_shader(GLenum shader_type, char const *source) const
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

    int gl_program::validate(GLuint param) const
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

    int gl_program::init()
    {
        vertex_shader_id = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader_id = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

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
        use();
        transform_location = get_uniform("transform");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_program::get_uniform(char const *name)
    {
        int location;
        GL_CHECK(location = glGetUniformLocation(program_id, name));
        if(location == static_cast<GLuint>(-1)) {
            LOG_WARNING("Can't get uniform location for \"{}\" in program \"{}\"", name, program_name);
        }
        return location;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_program::get_attribute(char const *name)
    {
        int location;
        GL_CHECK(location = glGetAttribLocation(program_id, name));
        if(location == static_cast<GLuint>(-1)) {
            LOG_ERROR("Can't get attribute location for \"{}\" in program \"{}\"", name, program_name);
        }
        return location;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program::use() const
    {
        GL_CHECK(glUseProgram(program_id));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_layer_program::init()
    {
        program_name = "layer";
        vertex_shader_source = layer_vertex_shader_source;
        fragment_shader_source = layer_fragment_shader_source;
        int err = gl_program::init();
        if(err != 0) {
            return err;
        }
        color_location = get_uniform("cover_in");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_layer_program::set_color(uint32_t cover) const
    {
        gl_color::float4 cov = gl_color::to_floats(cover);
        GL_CHECK(glUniform4f(color_location, cov[0], cov[1], cov[2], cov[3]));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_solid_program::init()
    {
        program_name = "solid";
        vertex_shader_source = solid_vertex_shader_source;
        fragment_shader_source = common_fragment_shader_source;
        int err = gl_program::init();
        if(err != 0) {
            return err;
        }
        color_location = get_uniform("uniform_color");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_solid_program::set_color(uint32_t solid_color) const
    {
        gl_color::float4 f = gl_color::to_floats(solid_color);
        GL_CHECK(glUniform4f(color_location, f[0], f[1], f[2], f[3]));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_color_program::init()
    {
        program_name = "color";
        vertex_shader_source = color_vertex_shader_source;
        fragment_shader_source = common_fragment_shader_source;
        return gl_program::init();
    }

    //////////////////////////////////////////////////////////////////////

    int gl_textured_program::init()
    {
        program_name = "textured";
        vertex_shader_source = textured_vertex_shader_source;
        fragment_shader_source = textured_fragment_shader_source;
        int err = gl_program::init();
        if(err != 0) {
            return err;
        }
        red_color_uniform = get_uniform("red_color");
        green_color_uniform = get_uniform("green_color");
        blue_color_uniform = get_uniform("blue_color");

        alpha_uniform = get_uniform("alpha");
        cover_sampler = get_uniform("cover_sampler");
        num_samples_uniform = get_uniform("num_samples");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program::cleanup()
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

    int gl_index_array::init(GLsizei index_count)
    {
        GL_CHECK(glGenBuffers(1, &ibo_id));
        num_indices = index_count;
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
        GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * num_indices, nullptr, GL_DYNAMIC_DRAW));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_index_array::activate() const
    {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_index_array::cleanup()
    {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        GLuint buffers[] = { ibo_id };
        GL_CHECK(glDeleteBuffers(static_cast<GLsizei>(gerber_util::array_length(buffers)), buffers));

        ibo_id = 0;
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

    int gl_vertex_array_solid::init(gl_program &program, GLsizei vert_count)
    {
        int err = alloc(vert_count, sizeof(gl_vertex_solid));
        if(err != 0) {
            return err;
        }
        position_location = program.get_attribute("position");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::init(gl_program &program, GLsizei vert_count)
    {
        int err = alloc(vert_count, sizeof(gl_vertex_color));
        if(err != 0) {
            return err;
        }
        position_location = program.get_attribute("position");
        color_location = program.get_attribute("vert_color_in");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_textured::init(gl_program &program, GLsizei vert_count)
    {
        int err = alloc(vert_count, sizeof(gl_vertex_textured));
        if(err != 0) {
            return err;
        }
        position_location = program.get_attribute("position");
        tex_coord_location = program.get_attribute("tex_coord_in");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::activate() const
    {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(position_location);
        glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_solid), static_cast<void *>(offsetof(gl_vertex_solid, x)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(position_location);
        glEnableVertexAttribArray(color_location);
        glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_color), static_cast<void *>(offsetof(gl_vertex_color, x)));
        glVertexAttribPointer(color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_vertex_color), reinterpret_cast<void *>(offsetof(gl_vertex_color, color)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_textured::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(position_location);
        glEnableVertexAttribArray(tex_coord_location);
        glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_textured), static_cast<void *>(offsetof(gl_vertex_textured, x)));
        glVertexAttribPointer(tex_coord_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_textured), reinterpret_cast<void *>(offsetof(gl_vertex_textured, u)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_vertex_array::cleanup()
    {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

        GLuint buffers[] = { vbo_id };
        GL_CHECK(glDeleteBuffers(static_cast<GLsizei>(gerber_util::array_length(buffers)), buffers));

        vbo_id = 0;
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
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        GLuint t[] = { texture_id };
        GL_CHECK(glDeleteTextures(1, t));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_render_target::init(GLuint new_width, GLuint new_height, GLuint multisample_count, GLuint slots)
    {
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
        for(GLuint slot = 0; slot < num_slots;  ++slot) {
            GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_TEXTURE_2D_MULTISAMPLE, texture_ids[slot], 0));
        }
        std::vector<GLenum> buffers;
        buffers.reserve(num_slots);
        GLenum a = GL_COLOR_ATTACHMENT0;
        for(GLuint i = 0; i < num_slots; ++i) {
            buffers.push_back(a);
            a += 1;
        }
        GL_CHECK(glNamedFramebufferDrawBuffers(fbo, num_slots, buffers.data()));
    }

    //////////////////////////////////////////////////////////////////////

    void gl_render_target::bind_textures() const
    {
        for(GLuint slot = 0; slot < num_slots;  ++slot) {
            GL_CHECK(glActiveTexture(GL_TEXTURE0 + slot));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture_ids[slot]));
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_render_target::cleanup()
    {
        GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

        GLuint *t = texture_ids.data();
        GL_CHECK(glDeleteTextures(num_slots, t));

        GLuint f[] = { fbo };
        GL_CHECK(glDeleteFramebuffers(1, f));
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawlist::draw()
    {
        if(!drawlist.empty()) {
            vertex_array.activate();
            gl_vertex_color *v;
            GL_CHECK(v = static_cast<gl_vertex_color*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY)));
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
