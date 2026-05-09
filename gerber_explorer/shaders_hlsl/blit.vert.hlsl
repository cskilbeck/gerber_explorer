// Fullscreen triangle vertex shader - generates vertices from SV_VertexID

struct VSOutput
{
    float4 pos : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    float x = -1.0 + float((vertex_id & 1) << 2);
    float y = -1.0 + float((vertex_id & 2) << 1);
    output.tex_coord.x = (x + 1.0) * 0.5;
    output.tex_coord.y = (y + 1.0) * 0.5;
    output.pos = float4(x, y, 0, 1);
    return output;
}
