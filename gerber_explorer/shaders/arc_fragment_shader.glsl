#version 410 core

in vec2 v_center_px;
in vec2 v_current_px;
in float v_radius_px;
in vec2 v_angles;// x = start, y = sweep

uniform float thickness;
uniform vec4 color;

out vec4 fragColor;

const float TWO_PI = 6.28318530718;

void main() {
    vec2 raw_dir = v_current_px - v_center_px;
    vec2 dir = vec2(raw_dir.x, -raw_dir.y);

    float dist_sq = dot(dir, dir);

    float half_thick = thickness * 0.5;
    float outer_r = v_radius_px + half_thick + 1.0;
    float inner_r = max(0, v_radius_px - half_thick - 1.0);

    // can it be in the arc at all?
    if (dist_sq > (outer_r * outer_r) || dist_sq < (inner_r * inner_r)) {
        discard;
    }

    // could do a 'full circle' early check here but most of them aren't full circles

    float d = sqrt(dist_sq);

    float ang = atan(dir.y, dir.x);
    float rel_ang = mod(ang - v_angles.x, TWO_PI);

    float final_dist;
    if (rel_ang <= v_angles.y) {
        final_dist = abs(d - v_radius_px);
    } else {
        vec2 p_start = v_radius_px * vec2(cos(v_angles.x), sin(v_angles.x));
        vec2 p_end   = v_radius_px * vec2(cos(v_angles.x + v_angles.y), sin(v_angles.x + v_angles.y));

        float d_start_sq = dot(dir - p_start, dir - p_start);
        float d_end_sq   = dot(dir - p_end, dir - p_end);
        final_dist = sqrt(min(d_start_sq, d_end_sq));
    }

    float alpha = clamp((half_thick - final_dist) + 0.5, 0.0, 1.0);

    if (alpha <= 0.0) {
        discard;
    }
    fragColor = vec4(color.rgb, color.a * alpha);
}
