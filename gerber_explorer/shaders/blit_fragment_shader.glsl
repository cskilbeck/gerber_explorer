in vec2 tex_coord_out;

out vec4 fragment;

uniform vec4 fill_color;
uniform vec4 other_color;
uniform int num_samples;
uniform sampler2DMS cover_sampler;

vec4 multisample(sampler2DMS sampler, ivec2 coord)
{
    vec4 color = vec4(0, 0, 0, 0);
    for (int i = 0; i < num_samples; i++) {
        color += texelFetch(sampler, coord, i);
    }
    return color / float(num_samples);
}

void main() {

    ivec2 tex_size = textureSize(cover_sampler);
    ivec2 uv = ivec2(tex_coord_out * tex_size);
    vec4 cover = multisample(cover_sampler, uv);
    float fill = cover.r;
    float clear = cover.g;
    float other = cover.b;
    float mask = clamp(fill - clear, 0, 1);
    float fill_a = mask * fill_color.a;
    float other_a = other * other_color.a;
    vec3 final_rgb = mix(fill_color.rgb, other_color.rgb, other);
    float final_alpha = mix(fill_a, other_a, other);
    fragment = vec4(final_rgb * final_alpha, final_alpha);
}
