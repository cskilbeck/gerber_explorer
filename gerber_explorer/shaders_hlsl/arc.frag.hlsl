// Arc fragment shader - distance field arc rendering with angle checking

static const float TWO_PI = 6.28318530718;

cbuffer Uniforms : register(b0, space3)
{
    float thickness;
    float _pad0;
    float _pad1;
    float _pad2;
    float4 color;
};

struct PSInput
{
    float4 pos : SV_Position;
    float2 v_center_px : TEXCOORD0;
    float2 v_current_px : TEXCOORD1;
    float v_radius_px : TEXCOORD2;
    float2 v_angles : TEXCOORD3;     // x = start, y = sweep
};

float4 main(PSInput input) : SV_Target
{
    float2 raw_dir = input.v_current_px - input.v_center_px;
    float2 dir = float2(raw_dir.x, -raw_dir.y);

    float dist_sq = dot(dir, dir);

    float half_thick = thickness * 0.5;
    float outer_r = input.v_radius_px + half_thick + 1.0;
    float inner_r = max(0, input.v_radius_px - half_thick - 1.0);

    if (dist_sq > (outer_r * outer_r) || dist_sq < (inner_r * inner_r)) {
        discard;
    }

    float d = sqrt(dist_sq);

    float ang = atan2(dir.y, dir.x);
    float rel_ang = fmod(ang - input.v_angles.x + TWO_PI * 2.0, TWO_PI);

    float final_dist;
    if (rel_ang <= input.v_angles.y) {
        final_dist = abs(d - input.v_radius_px);
    } else {
        float2 p_start = input.v_radius_px * float2(cos(input.v_angles.x), sin(input.v_angles.x));
        float2 p_end = input.v_radius_px * float2(cos(input.v_angles.x + input.v_angles.y), sin(input.v_angles.x + input.v_angles.y));

        float d_start_sq = dot(dir - p_start, dir - p_start);
        float d_end_sq = dot(dir - p_end, dir - p_end);
        final_dist = sqrt(min(d_start_sq, d_end_sq));
    }

    float alpha = clamp((half_thick - final_dist) + 0.5, 0.0, 1.0);

    if (alpha <= 0.0) {
        discard;
    }
    return float4(color.rgb, color.a * alpha);
}
