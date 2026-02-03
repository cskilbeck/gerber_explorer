out vec2 tex_coord_out;

void main() {
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    tex_coord_out.x = (x + 1.0) * 0.5;
    tex_coord_out.y = (y + 1.0) * 0.5;
    gl_Position = vec4(x, y, 0, 1);
}
