// Layer fragment shader - colors based on entity flag bitmasks

cbuffer Uniforms : register(b0, space3)
{
    int red_flags;
    int green_flags;
    int blue_flags;
    int _pad0;
    float4 value;
};

struct PSInput
{
    float4 pos : SV_Position;
    nointerpolation int entity_flags : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    if (input.entity_flags == 0) {
        discard;
    }
    float r = (float)((input.entity_flags & red_flags) != 0);
    float g = (float)((input.entity_flags & green_flags) != 0);
    float b = (float)((input.entity_flags & blue_flags) != 0);
    return float4(r * value.r, g * value.g, b * value.b, value.a);
}
