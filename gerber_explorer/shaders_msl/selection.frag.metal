// Selection fragment shader - RGB channel-based coverage rendering

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4 red_color;
    float4 green_color;
    float4 blue_color;
};

struct PSInput
{
    float4 pos [[position]];
    float2 tex_coord;
};

fragment float4 main0(
    PSInput in [[stage_in]],
    constant Uniforms& u [[buffer(0)]],
    texture2d<float> cover_texture [[texture(0)]],
    sampler cover_sampler [[sampler(0)]])
{
    float4 cover = cover_texture.sample(cover_sampler, in.tex_coord);
    float4 red = u.red_color * cover.r;
    float4 green = u.green_color * cover.g;
    float4 blue = u.blue_color * cover.b;
    return red + green + blue;
}
