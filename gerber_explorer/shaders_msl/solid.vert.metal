// Solid color vertex shader
// SDL_GPU bindings (Metal): vertex_uniform b0 -> [[buffer(0)]],
// vertex input buffer slot 0 -> [[buffer(14)]] (METAL_FIRST_VERTEX_BUFFER_SLOT).

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4x4 transform;
    float4 uniform_color;
};

struct VSInput
{
    float2 position [[attribute(0)]];
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
    out.color = u.uniform_color;
    return out;
}
