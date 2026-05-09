// Line2 fragment shader - multi-channel colored distance field lines

cbuffer Uniforms : register(b0, space3)
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
    float4 pos : SV_Position;
    float2 v_local_pos : TEXCOORD0;
    float v_length_px : TEXCOORD1;
    float4 v_color : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target
{
    float radius = thickness * 0.5;
    float half_len = input.v_length_px * 0.5;

    float proj = clamp(input.v_local_pos.x, -half_len, half_len);
    float dist = length(input.v_local_pos - float2(proj, 0.0));

    float alpha = 1.0 - smoothstep(radius - 1.0, radius + 1.0, dist);

    if (alpha <= 0.0) discard;

    float3 final_color = red_color.rgb * input.v_color.r + green_color.rgb * input.v_color.g + blue_color.rgb * input.v_color.b;

    return float4(final_color, alpha);
}
