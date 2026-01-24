uniform int red_flags;
uniform int green_flags;
uniform int blue_flags;

flat in int entity_flags;

out vec4 fragment;

void main() {
    vec4 color = vec4(0);
    if(entity_flags == 0) {
        discard;
    }
    if ((entity_flags & red_flags) != 0) {
        color += vec4(1, 0, 0, 1);
    }
    if ((entity_flags & green_flags) != 0) {
        color += vec4(0, 1, 0, 1);
    }
    if ((entity_flags & blue_flags) != 0) {
        color += vec4(0, 0, 1, 1);
    }
    fragment = color;
}
