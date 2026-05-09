// Instanced line vertex shader - generates screen-space quads for thick lines

cbuffer Uniforms : register(b0, space1)
{
    float4x4 transform;
    float2 viewport_size;
    float thickness;
    float _pad0;
};

struct VSInput
{
    float2 position : TEXCOORD0;      // Unit quad (-0.5 to 0.5)
    float2 posA : TEXCOORD1;          // Line start (instanced)
    float2 posB : TEXCOORD2;          // Line end (instanced)
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 v_local_pos : TEXCOORD0;
    float v_length_px : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 clipA = mul(transform, float4(input.posA, 0.0, 1.0));
    float4 clipB = mul(transform, float4(input.posB, 0.0, 1.0));
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
