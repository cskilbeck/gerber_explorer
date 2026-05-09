// Layer vertex shader with entity flag lookup via storage buffer

cbuffer Uniforms : register(b0, space1)
{
    float4x4 transform;
    int draw_flags;
    int _pad0;
    int _pad1;
    int _pad2;
};

// Entity flags storage buffer (replaces usamplerBuffer)
StructuredBuffer<uint> flags_buffer : register(t0, space0);

struct VSInput
{
    float2 position : TEXCOORD0;
    int entity_id : TEXCOORD1;
};

struct VSOutput
{
    float4 pos : SV_Position;
    nointerpolation int entity_flags : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.entity_flags = (int)flags_buffer[input.entity_id];
    if ((output.entity_flags & draw_flags) != 0) {
        output.pos = mul(transform, float4(input.position, 0.0f, 1.0f));
    } else {
        output.pos = float4(0, 0, 0, 0);
    }
    return output;
}
