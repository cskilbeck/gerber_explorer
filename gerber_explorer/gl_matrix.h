#pragma once

#include "gerber_2d.h"

namespace gerber_3d
{
    using gl_matrix = float[16];

    void set_identity(gl_matrix mat);
    void make_ortho(gl_matrix mat, int w, int h);
    void make_translate(gl_matrix mat, float x, float y);
    void make_scale(gl_matrix mat, float x_scale, float y_scale);

    void matrix_multiply(gl_matrix const a, gl_matrix const b, gl_matrix out);

    gerber_lib::gerber_2d::vec2f matrix_apply(gerber_lib::gerber_2d::vec2f const &v, gl_matrix const m);

}    // namespace gerber_3d
