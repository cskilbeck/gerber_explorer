// Instanced arc vertex shader - generates bounding box quads in screen space

cbuffer Uniforms : register(b0, space1)
{
    float4x4 transform;
    float2 viewport_size;
    float thickness;
    float _pad0;
};

struct VSInput
{
    float2 position : TEXCOORD0;      // Unit quad (0,0 to 1,1)
    float2 center : TEXCOORD1;        // Instanced
    float radius : TEXCOORD2;         // Instanced
    float start_angle : TEXCOORD3;    // Instanced
    float sweep : TEXCOORD4;          // Instanced
    float2 bbox_min : TEXCOORD5;      // Instanced (world space)
    float2 bbox_max : TEXCOORD6;      // Instanced (world space)
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 v_center_px : TEXCOORD0;
    float2 v_current_px : TEXCOORD1;
    float v_radius_px : TEXCOORD2;
    float2 v_angles : TEXCOORD3;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 clip_center = mul(transform, float4(input.center, 0.0, 1.0));
    output.v_center_px = (clip_center.xy / clip_center.w + 1.0) * 0.5 * viewport_size;

    float4 clip_edge = mul(transform, float4(input.center + float2(input.radius, 0.0), 0.0, 1.0));
    float2 screen_edge = (clip_edge.xy / clip_edge.w + 1.0) * 0.5 * viewport_size;

    output.v_radius_px = distance(output.v_center_px, screen_edge);

    float4 clip_min = mul(transform, float4(input.bbox_min, 0.0, 1.0));
    float4 clip_max = mul(transform, float4(input.bbox_max, 0.0, 1.0));

    float2 screenMin = (clip_min.xy + 1.0) * 0.5 * viewport_size;
    float2 screenMax = (clip_max.xy + 1.0) * 0.5 * viewport_size;

    float2 p_min = min(screenMin, screenMax);
    float2 p_max = max(screenMin, screenMax);

    float pad = thickness * 0.5 + 1.0;
    p_min -= float2(pad, pad);
    p_max += float2(pad, pad);

    float2 pixel_pos = lerp(p_min, p_max, input.position);
    output.v_current_px = pixel_pos;

    float2 ndc = (pixel_pos / viewport_size) * 2.0 - 1.0;
    output.pos = float4(ndc, 0.0, 1.0);

    output.v_angles = float2(input.start_angle, input.sweep);

    return output;
}
