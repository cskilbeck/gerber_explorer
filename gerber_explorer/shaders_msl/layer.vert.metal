// Layer vertex shader with entity flag lookup via storage buffer
// Bindings (vertex stage, 1 uniform + 1 storage buffer):
//   uniform b0   -> [[buffer(0)]]
//   storage buf0 -> [[buffer(1)]]   (vertexUniformBufferCount + 0)
//   vertex input -> [[buffer(14)]]  via [[stage_in]]

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4x4 transform;
    int draw_flags;
    int _pad0;
    int _pad1;
    int _pad2;
};

struct VSInput
{
    float2 position [[attribute(0)]];
    uint entity_id [[attribute(1)]];
};

struct VSOutput
{
    float4 pos [[position]];
    int entity_flags [[flat]];
};

vertex VSOutput main0(
    VSInput in [[stage_in]],
    constant Uniforms& u [[buffer(0)]],
    const device uint* flags_buffer [[buffer(1)]])
{
    VSOutput out;
    out.entity_flags = (int)flags_buffer[in.entity_id];
    if ((out.entity_flags & u.draw_flags) != 0) {
        out.pos = u.transform * float4(in.position, 0.0, 1.0);
    } else {
        out.pos = float4(0.0, 0.0, 0.0, 0.0);
    }
    return out;
}
