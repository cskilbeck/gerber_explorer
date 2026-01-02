#version 410 core

layout(location = 0) in vec2 position;

out vec4 cover;

uniform mat4 transform;
uniform vec4 cover_in;
uniform vec2 center;
uniform bool x_flip;
uniform bool y_flip;

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
    gl_Position = transform * vec4(flipper(position), 0.0f, 1.0f);
    cover = cover_in;
}
