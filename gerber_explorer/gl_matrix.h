#pragma once

namespace gerber_3d
{
    using gl_matrix = float[16];

    void make_ortho(gl_matrix mat, int w, int h);
    void make_translate(gl_matrix mat, float x, float y);
    void make_scale(gl_matrix mat, float x_scale, float y_scale);

    void matrix_multiply(gl_matrix const a, gl_matrix const b, gl_matrix out);

}    // namespace gerber_3d