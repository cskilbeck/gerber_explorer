layout(location = 0) in vec2 position;
layout(location = 1) in int entity_id;

uniform mat4 transform;
uniform usamplerBuffer flags_sampler;
uniform int draw_flags;

flat out int entity_flags;

void main() {
    entity_flags = int(texelFetch(flags_sampler, entity_id).r);
    if((entity_flags & draw_flags) != 0) {
        gl_Position = transform * vec4(position, 0.0f, 1.0f);
    } else {
        gl_Position = vec4(0);
    }
}
