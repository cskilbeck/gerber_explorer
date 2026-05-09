// Per-vertex color vertex shader

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4x4 transform;
};

struct VSInput
{
    float2 position [[attribute(0)]];
    float4 vert_color [[attribute(1)]];   // UBYTE4_NORM -> normalized float4
};

struct VSOutput
{
    float4 pos [[position]];
    float4 color;
};

vertex VSOutput main0(
    VSInput in [[stage_in]],
    constant Uniforms& u [[buffer(0)]])
{
    VSOutput out;
    out.pos = u.transform * float4(in.position, 0.0, 1.0);
    out.color = in.vert_color;
    return out;
}
