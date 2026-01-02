#version 410 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 vert_color;

out vec4 color;

uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(position, 0.0f, 1.0f);
    color = vert_color;
}
