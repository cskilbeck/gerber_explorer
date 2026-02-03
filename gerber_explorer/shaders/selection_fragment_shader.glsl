in vec2 tex_coord_out;

out vec4 fragment;

uniform vec4 red_color;
uniform vec4 green_color;
uniform vec4 blue_color;
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
    vec4 red = red_color * cover.r;
    vec4 green = green_color * cover.g;
    vec4 blue = blue_color * cover.b;
    fragment = red + green + blue;
}
