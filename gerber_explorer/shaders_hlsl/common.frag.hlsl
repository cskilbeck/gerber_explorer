// Pass-through fragment shader

struct PSInput
{
    float4 pos : SV_Position;
    float4 color : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return input.color;
}
