#pragma once

#include "gerber_2d.h"

namespace gerber_3d
{
    struct gl_matrix
    {
        float m[16];
    };

    gl_matrix make_identity();
    gl_matrix make_translate(float x, float y);
    gl_matrix make_scale(float x, float y);
    gl_matrix make_ortho(int w, int h);
    gl_matrix matrix_multiply(gl_matrix const &a, gl_matrix const &b);

    gerber_lib::gerber_2d::vec2f matrix_apply(gerber_lib::gerber_2d::vec2f const &v, gl_matrix const &matrix);

    gl_matrix make_2d_transform(int screen_w, int screen_h, const gerber_lib::gerber_2d::rect &view_rect,
                                        gerber_lib::gerber_2d::vec2f center, bool flip_x, bool flip_y);

}    // namespace gerber_3d
