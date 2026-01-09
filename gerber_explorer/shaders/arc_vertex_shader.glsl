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
