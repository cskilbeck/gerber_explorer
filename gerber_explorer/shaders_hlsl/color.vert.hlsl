// Per-vertex color vertex shader

cbuffer Uniforms : register(b0, space1)
{
    float4x4 transform;
};

struct VSInput
{
    float2 position : TEXCOORD0;
    float4 vert_color : TEXCOORD1;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 color : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = mul(transform, float4(input.position, 0.0f, 1.0f));
    output.color = input.vert_color;
    return output;
}
