// Layer fragment shader - colors based on entity flag bitmasks

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    int red_flags;
    int green_flags;
    int blue_flags;
    int _pad0;
    float4 value;
};

struct PSInput
{
    float4 pos [[position]];
    int entity_flags [[flat]];
};

fragment float4 main0(
    PSInput in [[stage_in]],
    constant Uniforms& u [[buffer(0)]])
{
    if (in.entity_flags == 0) {
        discard_fragment();
    }
    float r = (float)((in.entity_flags & u.red_flags) != 0);
    float g = (float)((in.entity_flags & u.green_flags) != 0);
    float b = (float)((in.entity_flags & u.blue_flags) != 0);
    return float4(r * u.value.r, g * u.value.g, b * u.value.b, u.value.a);
}
