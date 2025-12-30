#include "gl_matrix.h"

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    gl_matrix make_identity()
    {
        gl_matrix mat;
        mat.m[0] = 1;
        mat.m[1] = 0;
        mat.m[2] = 0;
        mat.m[3] = 0;
        mat.m[4] = 0;
        mat.m[5] = 1;
        mat.m[6] = 0;
        mat.m[7] = 0;
        mat.m[8] = 0;
        mat.m[9] = 0;
        mat.m[10] = 1;
        mat.m[11] = 0;
        mat.m[12] = 0;
        mat.m[13] = 0;
        mat.m[14] = 0;
        mat.m[15] = 1;
        return mat;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix make_ortho(int w, int h)
    {
        gl_matrix mat;
        mat.m[0] = 2.0f / w;
        mat.m[1] = 0.0f;
        mat.m[2] = 0.0f;
        mat.m[3] = -1.0f;
        mat.m[4] = 0.0f;
        mat.m[5] = 2.0f / h;
        mat.m[6] = 0.0f;
        mat.m[7] = -1.0f;
        mat.m[8] = 0.0f;
        mat.m[9] = 0.0f;
        mat.m[10] = -1.0f;
        mat.m[11] = 0.0f;
        mat.m[12] = 0.0f;
        mat.m[13] = 0.0f;
        mat.m[14] = 0.0f;
        mat.m[15] = 1.0f;
        return mat;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix make_translate(float x, float y)
    {
        gl_matrix mat;
        mat.m[0] = 1.0f;
        mat.m[1] = 0.0f;
        mat.m[2] = 0.0f;
        mat.m[3] = x;
        mat.m[4] = 0.0f;
        mat.m[5] = 1.0f;
        mat.m[6] = 0.0f;
        mat.m[7] = y;
        mat.m[8] = 0.0f;
        mat.m[9] = 0.0f;
        mat.m[10] = 1.0f;
        mat.m[11] = 0.0f;
        mat.m[12] = 0.0f;
        mat.m[13] = 0.0f;
        mat.m[14] = 0.0f;
        mat.m[15] = 1.0f;
        return mat;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix make_scale(float x_scale, float y_scale)
    {
        gl_matrix mat;
        mat.m[0] = x_scale;
        mat.m[1] = 0.0f;
        mat.m[2] = 0.0f;
        mat.m[3] = 0.0f;
        mat.m[4] = 0.0f;
        mat.m[5] = y_scale;
        mat.m[6] = 0.0f;
        mat.m[7] = 0.0f;
        mat.m[8] = 0.0f;
        mat.m[9] = 0.0f;
        mat.m[10] = 1.0f;
        mat.m[11] = 0.0f;
        mat.m[12] = 0.0f;
        mat.m[13] = 0.0f;
        mat.m[14] = 0.0f;
        mat.m[15] = 1.0f;
        return mat;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix matrix_multiply(gl_matrix const &a, gl_matrix const &b)
    {
        gl_matrix t;
        for(int i = 0; i < 16; i += 4) {
            t.m[i + 0] = a.m[i] * b.m[0] + a.m[i + 1] * b.m[4] + a.m[i + 2] * b.m[8] + a.m[i + 3] * b.m[12];
            t.m[i + 1] = a.m[i] * b.m[1] + a.m[i + 1] * b.m[5] + a.m[i + 2] * b.m[9] + a.m[i + 3] * b.m[13];
            t.m[i + 2] = a.m[i] * b.m[2] + a.m[i + 1] * b.m[6] + a.m[i + 2] * b.m[10] + a.m[i + 3] * b.m[14];
            t.m[i + 3] = a.m[i] * b.m[3] + a.m[i + 1] * b.m[7] + a.m[i + 2] * b.m[11] + a.m[i + 3] * b.m[15];
        }
        return t;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_lib::gerber_2d::vec2f matrix_apply(gerber_lib::gerber_2d::vec2f const &v, gl_matrix const &matrix)
    {
        float x = v.x * matrix.m[0] + v.y * matrix.m[4] + matrix.m[12];
        float y = v.x * matrix.m[1] + v.y * matrix.m[5] + matrix.m[13];
        return gerber_lib::gerber_2d::vec2f{ x, y };
    }

}    // namespace gerber_3d
