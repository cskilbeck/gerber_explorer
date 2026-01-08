#version 410 core

layout(location = 0) in vec2 position;

out vec4 color;

uniform mat4 transform;
uniform vec4 u_color;

void main() {
    gl_Position = transform * vec4(position, 0.0f, 1.0f);
    color = u_color;
}
