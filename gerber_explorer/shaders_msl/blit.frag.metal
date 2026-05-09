// Blit fragment shader - composites resolved coverage texture with fill/clear/other colors
// Bindings (fragment stage, 1 sampler + 1 uniform buffer):
//   uniform b0       -> [[buffer(0)]]
//   sampled texture  -> [[texture(0)]]
//   sampler          -> [[sampler(0)]]

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4 fill_color;
    float4 other_color;
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
    float fill = cover.r;
    float clear_v = cover.g;
    float other = cover.b;
    float mask = clamp(fill - clear_v, 0.0, 1.0);
    float fill_a = mask * u.fill_color.a;
    float other_a = other * u.other_color.a;
    float3 final_rgb = mix(u.fill_color.rgb, u.other_color.rgb, other);
    float final_alpha = mix(fill_a, other_a, other);
    return float4(final_rgb * final_alpha, final_alpha);
}
