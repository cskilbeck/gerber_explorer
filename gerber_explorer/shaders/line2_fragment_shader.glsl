in vec2 v_local_pos;
in float v_length_px;
in vec4 v_color;

uniform float thickness;

uniform vec4 red_color;
uniform vec4 green_color;
uniform vec4 blue_color;

out vec4 fragColor;

void main() {
    float radius = thickness * 0.5;
    float half_len = v_length_px * 0.5;

    // Find the closest point on the segment line
    float proj = clamp(v_local_pos.x, -half_len, half_len);
    float dist = length(v_local_pos - vec2(proj, 0.0));

    float alpha = 1.0 - smoothstep(radius - 1.0, radius + 1, dist);

    if (alpha <= 0) discard;

    vec3 final_color = red_color.rgb * v_color.r + green_color.rgb * v_color.g + blue_color.rgb * v_color.b;

    fragColor = vec4(final_color, alpha);
}
