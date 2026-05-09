// Advanced instanced line vertex shader with storage buffer lookups
// Replaces 3 TBOs (instance data, vertex positions, entity flags) with storage buffers

cbuffer Uniforms : register(b0, space1)
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

// Storage buffers (replace TBOs)
StructuredBuffer<LineInstance> instance_buffer : register(t0, space0);
StructuredBuffer<float2> vert_buffer : register(t1, space0);
StructuredBuffer<uint> flags_buffer : register(t2, space0);

struct VSInput
{
    float2 position : TEXCOORD0;  // Unit quad (-0.5 to 0.5)
    uint instance_id : SV_InstanceID;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 v_local_pos : TEXCOORD0;
    float v_length_px : TEXCOORD1;
    float4 v_color : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.v_color = float4(0, 0, 0, 0);
    output.v_local_pos = float2(0, 0);
    output.v_length_px = 0;

    LineInstance inst = instance_buffer[input.instance_id];

    uint flags = flags_buffer[inst.entity_id];

    if ((flags & (red_flag | green_flag | blue_flag)) == 0) {
        output.pos = float4(0.0, 0.0, 0.0, 0.0);
        return output;
    }

    if (flags & red_flag) output.v_color += float4(1, 0, 0, 0);
    if (flags & green_flag) output.v_color += float4(0, 1, 0, 0);
    if (flags & blue_flag) output.v_color += float4(0, 0, 1, 0);

    float2 posA = vert_buffer[inst.start_index];
    float2 posB = vert_buffer[inst.end_index];

    float4 clipA = mul(transform, float4(posA, 0.0, 1.0));
    float4 clipB = mul(transform, float4(posB, 0.0, 1.0));
    float2 screenA = (clipA.xy + 1.0) * 0.5 * viewport_size;
    float2 screenB = (clipB.xy + 1.0) * 0.5 * viewport_size;

    float2 dir = screenB - screenA;
    output.v_length_px = length(dir);
    float2 unit_dir = (output.v_length_px > 0.0) ? dir / output.v_length_px : float2(1.0, 0.0);
    float2 unit_normal = float2(-unit_dir.y, unit_dir.x);

    float quad_width_px = output.v_length_px + thickness;
    float quad_height_px = thickness;

    float2 screen_mid = (screenA + screenB) * 0.5;
    float2 screen_pos = screen_mid + (unit_dir * input.position.x * quad_width_px) + (unit_normal * input.position.y * quad_height_px);

    float2 normalized_pos = (screen_pos / viewport_size) * 2.0 - 1.0;
    output.pos = float4(normalized_pos, 0.0, 1.0);

    output.v_local_pos = float2(input.position.x * quad_width_px, input.position.y * quad_height_px);

    return output;
}
