// Blit fragment shader - composites resolved coverage texture with fill/clear/other colors
// SDL_GPU automatic MSAA resolve means we sample a regular Texture2D (not Texture2DMS)

cbuffer Uniforms : register(b0, space3)
{
    float4 fill_color;
    float4 other_color;
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
    float fill = cover.r;
    float clear = cover.g;
    float other = cover.b;
    float mask = clamp(fill - clear, 0, 1);
    float fill_a = mask * fill_color.a;
    float other_a = other * other_color.a;
    float3 final_rgb = lerp(fill_color.rgb, other_color.rgb, other);
    float final_alpha = lerp(fill_a, other_a, other);
    return float4(final_rgb * final_alpha, final_alpha);
}
