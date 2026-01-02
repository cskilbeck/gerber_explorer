#version 410 core

layout(location = 0) in vec2 position;

out vec4 cover;

uniform mat4 transform;
uniform vec4 cover_in;
uniform vec2 center;
uniform bool x_flip;
uniform bool y_flip;

void main() {
    gl_Position = transform * vec4(position, 0.0f, 1.0f);
    cover = cover_in;
}
