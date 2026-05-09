// Selection fragment shader - RGB channel-based coverage rendering
// SDL_GPU automatic MSAA resolve means we sample a regular Texture2D (not Texture2DMS)

cbuffer Uniforms : register(b0, space3)
{
    float4 red_color;
    float4 green_color;
    float4 blue_color;
};

Texture2D<float4> cover_texture : register(t0, space2);
SamplerState cover_sampler : register(s0, space2);

struct PSInput
{
    float4 pos : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float4 cover = cover_texture.Sample(cover_sampler, input.tex_coord);
    float4 red = red_color * cover.r;
    float4 green = green_color * cover.g;
    float4 blue = blue_color * cover.b;
    return red + green + blue;
}
