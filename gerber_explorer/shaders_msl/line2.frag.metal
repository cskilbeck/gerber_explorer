// Line2 fragment shader - multi-channel colored distance field lines

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float thickness;
    float _pad0;
    float _pad1;
    float _pad2;
    float4 red_color;
    float4 green_color;
    float4 blue_color;
};

struct PSInput
{
    float4 pos [[position]];
    float2 v_local_pos;
    float v_length_px;
    float4 v_color;
};

fragment float4 main0(
    PSInput in [[stage_in]],
    constant Uniforms& u [[buffer(0)]])
{
    float radius = u.thickness * 0.5;
    float half_len = in.v_length_px * 0.5;

    float t = saturate(in.v_length_px / u.thickness);
    float effective_radius = radius - (1.0 - t);

    float proj = clamp(in.v_local_pos.x, -half_len, half_len);
    float dist = length(in.v_local_pos - float2(proj, 0.0));

    float alpha = 1.0 - smoothstep(effective_radius - 1.0, effective_radius + 1.0, dist);

    if (alpha <= 0.0) discard_fragment();

    float3 final_color = u.red_color.rgb * in.v_color.r
                       + u.green_color.rgb * in.v_color.g
                       + u.blue_color.rgb * in.v_color.b;

    return float4(final_color, alpha);
}
