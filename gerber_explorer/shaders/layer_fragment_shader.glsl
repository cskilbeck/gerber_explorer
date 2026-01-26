uniform int red_flags;
uniform int green_flags;
uniform int blue_flags;

uniform vec4 value;

flat in int entity_flags;

out vec4 fragment;

void main() {
    if(entity_flags == 0) {
        discard;
    }
    float r = float((entity_flags & red_flags) != 0);
    float g = float((entity_flags & green_flags) != 0);
    float b = float((entity_flags & blue_flags) != 0);
    fragment = vec4(r * value.r, g * value.g, b * value.b, value.a);
}
