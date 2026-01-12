#version 410 core

in vec2 v_local_pos;
in float v_length_px;
in vec4 v_color_a;
in vec4 v_color_b;

uniform float thickness;
uniform float check_size;
uniform vec2 check_offset;

out vec4 fragColor;

void main() {
    float radius = thickness * 0.5;
    float half_len = v_length_px * 0.5;

    // Find the closest point on the segment line
    float proj = clamp(v_local_pos.x, -half_len, half_len);
    float dist = length(v_local_pos - vec2(proj, 0.0));

    float alpha = 1.0 - smoothstep(radius - 1.0, radius, dist);

    if (alpha <= 0.0) discard;

    vec2 pos = gl_FragCoord.xy + check_offset;
    vec2 cell = floor(pos / check_size);
    float checker = mod(cell.x + cell.y, 2.0);

    vec3 col = mix(v_color_a.rgb, v_color_b.rgb, checker);
    fragColor = vec4(col.rgb, 1.0f);
}
