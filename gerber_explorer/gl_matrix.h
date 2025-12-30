#pragma once

#include "gerber_2d.h"

namespace gerber_3d
{
    struct gl_matrix
    {
        float m[16];
    };

    gl_matrix make_identity();
    gl_matrix make_ortho(int w, int h);
    gl_matrix make_translate(float x, float y);
    gl_matrix make_scale(float x_scale, float y_scale);
    gl_matrix matrix_multiply(gl_matrix const &a, gl_matrix const &b);

    gerber_lib::gerber_2d::vec2f matrix_apply(gerber_lib::gerber_2d::vec2f const &v, gl_matrix const &matrix);

}    // namespace gerber_3d
