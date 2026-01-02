#version 410 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 tex_coord;

out vec2 tex_coord_out;

uniform mat4 transform;

void main() {
    vec2 pos = position;
    gl_Position = transform * vec4(pos.xy, 0.0f, 1.0f);
    tex_coord_out = tex_coord;
}
