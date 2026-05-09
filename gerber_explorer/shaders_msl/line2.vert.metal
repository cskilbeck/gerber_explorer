// Advanced instanced line vertex shader with storage buffer lookups
// Bindings (vertex stage, 1 uniform + 3 storage buffers):
//   uniform        b0 -> [[buffer(0)]]
//   instance_buffer   -> [[buffer(1)]]
//   vert_buffer       -> [[buffer(2)]]
//   flags_buffer      -> [[buffer(3)]]
//   vertex input      -> [[buffer(14)]]

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4x4 transform;
    float2 viewport_size;
    float thickness;
    uint red_flag;
    uint green_flag;
    uint blue_flag;
    float _pad0;
    float _pad1;
};

struct LineInstance
{
    uint start_index;
    uint end_index;
    uint entity_id;
    uint pad;
};

struct VSInput
{
    float2 position [[attribute(0)]];
};

struct VSOutput
{
    float4 pos [[position]];
    float2 v_local_pos;
    float v_length_px;
    float4 v_color;
};

vertex VSOutput main0(
    VSInput in [[stage_in]],
    uint instance_id [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device LineInstance* instance_buffer [[buffer(1)]],
    const device float2* vert_buffer [[buffer(2)]],
    const device uint* flags_buffer [[buffer(3)]])
{
    VSOutput out;
    out.v_color = float4(0.0, 0.0, 0.0, 0.0);
    out.v_local_pos = float2(0.0, 0.0);
    out.v_length_px = 0.0;

    LineInstance inst = instance_buffer[instance_id];
    uint flags = flags_buffer[inst.entity_id];

    if ((flags & (u.red_flag | u.green_flag | u.blue_flag)) == 0u) {
        out.pos = float4(0.0, 0.0, 0.0, 0.0);
        return out;
    }

    if (flags & u.red_flag)   out.v_color += float4(1.0, 0.0, 0.0, 0.0);
    if (flags & u.green_flag) out.v_color += float4(0.0, 1.0, 0.0, 0.0);
    if (flags & u.blue_flag)  out.v_color += float4(0.0, 0.0, 1.0, 0.0);

    float2 posA = vert_buffer[inst.start_index];
    float2 posB = vert_buffer[inst.end_index];

    float4 clipA = u.transform * float4(posA, 0.0, 1.0);
    float4 clipB = u.transform * float4(posB, 0.0, 1.0);
    float2 screenA = (clipA.xy + 1.0) * 0.5 * u.viewport_size;
    float2 screenB = (clipB.xy + 1.0) * 0.5 * u.viewport_size;

    float2 dir = screenB - screenA;
    out.v_length_px = length(dir);
    float2 unit_dir = (out.v_length_px > 0.0) ? dir / out.v_length_px : float2(1.0, 0.0);
    float2 unit_normal = float2(-unit_dir.y, unit_dir.x);

    float quad_width_px = out.v_length_px + u.thickness;
    float quad_height_px = u.thickness;

    float2 screen_mid = (screenA + screenB) * 0.5;
    float2 screen_pos = screen_mid
        + (unit_dir * in.position.x * quad_width_px)
        + (unit_normal * in.position.y * quad_height_px);

    float2 normalized_pos = (screen_pos / u.viewport_size) * 2.0 - 1.0;
    out.pos = float4(normalized_pos, 0.0, 1.0);

    out.v_local_pos = float2(in.position.x * quad_width_px, in.position.y * quad_height_px);
    return out;
}
