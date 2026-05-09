// Fullscreen triangle vertex shader - generates vertices from vertex_id

#include <metal_stdlib>
using namespace metal;

struct VSOutput
{
    float4 pos [[position]];
    float2 tex_coord;
};

vertex VSOutput main0(uint vertex_id [[vertex_id]])
{
    VSOutput out;
    float x = -1.0 + float((vertex_id & 1u) << 2);
    float y = -1.0 + float((vertex_id & 2u) << 1);
    out.tex_coord.x = (x + 1.0) * 0.5;
    out.tex_coord.y = (y + 1.0) * 0.5;
    out.pos = float4(x, y, 0.0, 1.0);
    return out;
}
