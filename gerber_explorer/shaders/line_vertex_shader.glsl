#version 410 core

layout (location = 0) in vec2 posA;// Instanced
layout (location = 1) in vec2 posB;// Instanced
layout (location = 2) in vec2 position;// (-0.5, -0.5) to (0.5, 0.5)

uniform mat4 transform;
uniform vec2 viewport_size;
uniform float thickness;

out vec2 v_local_pos;
out float v_length_px;

void main() {
    // points to pixels
    vec4 clipA = transform * vec4(posA, 0.0, 1.0);
    vec4 clipB = transform * vec4(posB, 0.0, 1.0);
    vec2 screenA = (clipA.xy + 1.0) * 0.5 * viewport_size;
    vec2 screenB = (clipB.xy + 1.0) * 0.5 * viewport_size;

    // direction and length
    vec2 dir = screenB - screenA;
    v_length_px = length(dir);
    vec2 unit_dir = (v_length_px > 0.0) ? dir / v_length_px : vec2(1.0, 0.0);
    vec2 unit_normal = vec2(-unit_dir.y, unit_dir.x);

    float quad_width_px = v_length_px + thickness;
    float quad_height_px = thickness;

    vec2 screen_mid = (screenA + screenB) * 0.5;
    vec2 screen_pos = screen_mid + (unit_dir * position.x * quad_width_px) + (unit_normal * position.y * quad_height_px);

    vec2 normalized_pos = (screen_pos / viewport_size) * 2.0 - 1.0;
    gl_Position = vec4(normalized_pos, 0.0, 1.0);

    // Local pos for the fragment shader (distance-based AA)
    v_local_pos = vec2(position.x * quad_width_px, position.y * quad_height_px);
}
