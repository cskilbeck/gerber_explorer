#version 410 core

in vec2 v_local_pos;
in float v_length;

uniform float thickness;
uniform vec4 color;

out vec4 fragColor;

void main() {
    float radius = thickness * 0.5;
    float half_len = v_length * 0.5;

    // Find the closest point on the segment line
    float proj = clamp(v_local_pos.x, -half_len, half_len);
    float dist = length(v_local_pos - vec2(proj, 0.0));

    // Calculate anti-aliasing based on screen-space derivative
    // This ensures the 1-pixel blur stays 1-pixel wide on screen
    float afwidth = fwidth(dist);
    float alpha = 1.0 - smoothstep(radius - afwidth, radius, dist);

    if (alpha <= 0.0) discard;

    fragColor = vec4(color.rgb, color.a * alpha);
}
