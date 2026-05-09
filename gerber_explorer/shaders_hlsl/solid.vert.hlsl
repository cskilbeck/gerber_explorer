// Solid color vertex shader
// Transforms 2D positions and applies a uniform color

cbuffer Uniforms : register(b0, space1)
{
    float4x4 transform;
    float4 uniform_color;
};

struct VSInput
{
    float2 position : TEXCOORD0;
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
    output.color = uniform_color;
    return output;
}
