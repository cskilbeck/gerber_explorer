layout (location = 0) in vec2 position;// (-0.5, -0.5) to (0.5, 0.5)

uniform mat4 transform;
uniform float thickness;// global line thickness
uniform vec2 viewport_size;// needed for pixel thickness
uniform vec4 hover_color;// coverage colors for hovered
uniform vec4 select_color;// and selected lines

uniform usamplerBuffer instance_sampler;// GL_TEXTURE0: sampler for TBO of struct lines, fetch based on gl_InstanceId (GL_RGB32UI)
uniform samplerBuffer vert_sampler;// GL_TEXTURE1: sampler for the vertices (GL_RG32F) [start_index, end_index]
uniform usamplerBuffer flags_sampler;// GL_TEXTURE2: sampler for TBO of flags [entity_id]

out vec2 v_local_pos;
out float v_length_px;
out vec4 v_color;

void main() {

    uvec4 instance = texelFetch(instance_sampler, gl_InstanceID).rgba;

    uint start_index = instance.r;
    uint end_index = instance.g;
    uint entity_id = instance.b;

    // get flags from flags_sampler via entity_id
    uint flags = texelFetch(flags_sampler, int(entity_id)).r;
    if (flags == 0) {
        gl_Position = vec4(0.0);
        return;
    }
    // set color channels based on flags
    float hover = 0;
    float select = 0;
    if((flags & 4u) != 0) {
        hover = 1;
    }
    if((flags & 8u) != 0) {
        select = 1;
    }
    v_color = mix(vec4(0), hover_color, hover);
    v_color = mix(v_color, select_color, select);

    // get verts from vert_sampler via start/end index
    vec2 posA = texelFetch(vert_sampler, int(start_index)).rg;
    vec2 posB = texelFetch(vert_sampler, int(end_index)).rg;

    // points to pixels
    vec4 clipA = transform * vec4(posA, 0.0, 1.0);
    vec4 clipB = transform * vec4(posB, 0.0, 1.0);
    vec2 screenA = (clipA.xy + 1.0) * 0.5 * viewport_size;
    vec2 screenB = (clipB.xy + 1.0) * 0.5 * viewport_size;

    // direction and length
    vec2 dir = screenB - screenA;
    v_length_px = length(dir);
    vec2 unit_dir = (v_length_px > 0.0) ? dir / v_length_px : vec2(1.0, 0.0);
    vec2 unit_normal = vec2(-unit_dir.y, unit_dir.x);

    float quad_width_px = v_length_px + thickness;
    float quad_height_px = thickness;

    vec2 screen_mid = (screenA + screenB) * 0.5;
    vec2 screen_pos = screen_mid + (unit_dir * position.x * quad_width_px) + (unit_normal * position.y * quad_height_px);

    vec2 normalized_pos = (screen_pos / viewport_size) * 2.0 - 1.0;
    gl_Position = vec4(normalized_pos, 0.0, 1.0);

    // Local pos for the fragment shader (distance-based AA)
    v_local_pos = vec2(position.x * quad_width_px, position.y * quad_height_px);
}
