#version 410 core

layout (location = 0) in vec2 position;// Unit quad (0,0 to 1,1)
layout (location = 1) in vec2 center;// Instanced
layout (location = 2) in float radius;// Instanced
layout (location = 3) in float start_angle;// Instanced
layout (location = 4) in float sweep;// Instanced
layout (location = 5) in vec2 bbox_min;// Instanced (World Space)
layout (location = 6) in vec2 bbox_max;// Instanced (World Space)

uniform mat4 transform;
uniform vec2 viewport_size;
uniform float thickness;

out vec2 v_center_px;
out vec2 v_current_px;
out float v_radius_px;
out vec2 v_angles;

void main() {

    vec4 clip_center = transform * vec4(center, 0.0, 1.0);
    v_center_px = (clip_center.xy / clip_center.w + 1.0) * 0.5 * viewport_size;

    vec4 clip_edge = transform * vec4(center + vec2(radius, 0.0), 0.0, 1.0);
    vec2 screen_edge = (clip_edge.xy / clip_edge.w + 1.0) * 0.5 * viewport_size;

    v_radius_px = distance(v_center_px, screen_edge);

    vec4 clip_min = transform * vec4(bbox_min, 0.0, 1.0);
    vec4 clip_max = transform * vec4(bbox_max, 0.0, 1.0);

    vec2 screenMin = (clip_min.xy + 1.0) * 0.5 * viewport_size;
    vec2 screenMax = (clip_max.xy + 1.0) * 0.5 * viewport_size;

    vec2 p_min = min(screenMin, screenMax);
    vec2 p_max = max(screenMin, screenMax);

    float pad = thickness * 0.5 + 1.0;// 0.5 for stroke + 1.0 pixel safety for AA
    p_min -= vec2(pad);
    p_max += vec2(pad);

    vec2 pixel_pos = mix(p_min, p_max, position);
    v_current_px = pixel_pos;

    vec2 ndc = (pixel_pos / viewport_size) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    v_angles = vec2(start_angle, sweep);
}


/*

    double radius = 40;
    vec2d center{ 50, 50 };

    if(glfwGetKey(window, GLFW_KEY_LEFT) != 0) {
        end_angle -= 1;
    }
    if(glfwGetKey(window, GLFW_KEY_RIGHT) != 0) {
        end_angle += 1;
    }
    if(glfwGetKey(window, GLFW_KEY_UP) != 0) {
        start_angle += 1;
    }
    if(glfwGetKey(window, GLFW_KEY_DOWN) != 0) {
        start_angle -= 1;
    }
    if(glfwGetKey(window, GLFW_KEY_SPACE) != 0) {
        start_angle = 0;
        end_angle = 190;
    }

    arc_extent = get_arc_extents(center, radius, start_angle, end_angle);

    std::array<gl_arc_program::arc, 1> arcs = {
        vec2f(center), (float)radius, gerber_lib::deg_2_radf(start_angle), gerber_lib::deg_2_radf(end_angle - start_angle), vec2f(arc_extent.min_pos), vec2f(arc_extent.max_pos)
    };

    arc_program.use();
    arc_program.quad_points_array.activate();

    using arc = gl_arc_program::arc;

    static GLuint arc_vbo;
    static bool init = false;
    if(!init) {
        init = true;
        GL_CHECK(glGenBuffers(1, &arc_vbo));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, arc_vbo));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(arc) * arcs.size(), nullptr, GL_DYNAMIC_DRAW));
    }

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, arc_vbo));
    update_buffer<GL_ARRAY_BUFFER>(arcs);

    arc_program.set_color(0xff00ff00);
    GL_CHECK(glUniform1f(arc_program.u_thickness, 22.0f));
    GL_CHECK(glUniform2f(arc_program.u_viewport_size, (float)window_size.x, (float)window_size.y));
    GL_CHECK(glUniformMatrix4fv(arc_program.u_transform, 1, false, world_matrix.m));

    GL_CHECK(glVertexAttribPointer(gl_arc_program::center_location, 2, GL_FLOAT, GL_FALSE, sizeof(arc), (void *)(offsetof(arc, center))));
    GL_CHECK(glVertexAttribPointer(gl_arc_program::radius_location, 1, GL_FLOAT, GL_FALSE, sizeof(arc), (void *)(offsetof(arc, radius))));
    GL_CHECK(glVertexAttribPointer(gl_arc_program::start_angle_location, 1, GL_FLOAT, GL_FALSE, sizeof(arc), (void *)(offsetof(arc, start_angle))));
    GL_CHECK(glVertexAttribPointer(gl_arc_program::sweep_location, 1, GL_FLOAT, GL_FALSE, sizeof(arc), (void *)(offsetof(arc, sweep))));
    GL_CHECK(glVertexAttribPointer(gl_arc_program::extent_min_location, 2, GL_FLOAT, GL_FALSE, sizeof(arc), (void *)(offsetof(arc, extent_min))));
    GL_CHECK(glVertexAttribPointer(gl_arc_program::extent_max_location, 2, GL_FLOAT, GL_FALSE, sizeof(arc), (void *)(offsetof(arc, extent_max))));
    GL_CHECK(glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)arcs.size()));

*/
