// Pass-through fragment shader

#include <metal_stdlib>
using namespace metal;

struct PSInput
{
    float4 pos [[position]];
    float4 color;
};

fragment float4 main0(PSInput in [[stage_in]])
{
    return in.color;
}
