#version 410 core

layout (location = 0) in vec2 posA;         // Instanced
layout (location = 1) in vec2 posB;         // Instanced
layout (location = 2) in vec2 position; // (-0.5, -0.5) to (0.5, 0.5)

uniform mat4 transform;
uniform vec2 center;
uniform bool x_flip;
uniform bool y_flip;
uniform float thickness;

out vec2 v_local_pos;
out float v_length;

vec2 flipper(vec2 p) {
    vec2 pos = p;
    if(x_flip) {
        pos.x = center.x - (pos.x - center.x);
    }
    if(y_flip) {
        pos.y = center.y - (pos.y - center.y);
    }
    return pos;
}

void main() {
    vec2 posa = flipper(posA);
    vec2 posb = flipper(posB);
    vec2 dir = posb - posa;
    v_length = length(dir);

    // Normalized direction and perpendicular normal
    vec2 segment_dir = (v_length > 0.0) ? dir / v_length : vec2(1.0, 0.0);
    vec2 norm = vec2(-segment_dir.y, segment_dir.x);

    // Stretch the quad to cover the capsule (length + 2*radius, width = 2*radius)
    float quad_width = v_length + thickness;
    float quad_height = thickness;

    vec2 center = (posa + posb) * 0.5;

    // Position the vertex in world space
    vec2 world_pos = center
                   + (segment_dir * position.x * quad_width)
                   + (norm * position.y * quad_height);

    // Map local coordinates: X goes from -half_width to +half_width
    v_local_pos = vec2(position.x * quad_width, position.y * quad_height);

    gl_Position = transform * vec4(world_pos, 0.0, 1.0);
}
