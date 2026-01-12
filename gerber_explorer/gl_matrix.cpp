#include "gl_matrix.h"

namespace gerber_3d
{
    using gerber_lib::vec2f;
    using gerber_lib::vec2d;

    //////////////////////////////////////////////////////////////////////

    gl_matrix make_identity()
    {
        gl_matrix t{};
        t.m[0] = 1.0f;
        t.m[5] = 1.0f;
        t.m[10] = 1.0f;
        t.m[15] = 1.0f;
        return t;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix make_translate(float x, float y)
    {
        gl_matrix t = make_identity();
        t.m[12] = x;
        t.m[13] = y;
        return t;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix make_scale(float x, float y)
    {
        gl_matrix t = make_identity();
        t.m[0] = x;
        t.m[5] = y;
        return t;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix make_ortho(int w, int h)
    {
        gl_matrix t{};
        t.m[0] = 2.0f / w;
        t.m[5] = 2.0f / h;
        t.m[10] = -1.0f;
        t.m[12] = -1.0f;
        t.m[13] = -1.0f;
        t.m[15] = 1.0f;
        return t;
    }

    //////////////////////////////////////////////////////////////////////

    gl_matrix matrix_multiply(gl_matrix const &a, gl_matrix const &b)
    {
        gl_matrix t;
        for(int c = 0; c < 16; c += 4) {
            t.m[c + 0] = a.m[0] * b.m[c + 0] + a.m[4] * b.m[c + 1] + a.m[8] * b.m[c + 2] + a.m[12] * b.m[c + 3];
            t.m[c + 1] = a.m[1] * b.m[c + 0] + a.m[5] * b.m[c + 1] + a.m[9] * b.m[c + 2] + a.m[13] * b.m[c + 3];
            t.m[c + 2] = a.m[2] * b.m[c + 0] + a.m[6] * b.m[c + 1] + a.m[10] * b.m[c + 2] + a.m[14] * b.m[c + 3];
            t.m[c + 3] = a.m[3] * b.m[c + 0] + a.m[7] * b.m[c + 1] + a.m[11] * b.m[c + 2] + a.m[15] * b.m[c + 3];
        }
        return t;
    }

    //////////////////////////////////////////////////////////////////////

    vec2f matrix_apply(vec2f const &v, gl_matrix const &matrix)
    {
        vec2f result;
        result.x = matrix.m[0] * v.x + matrix.m[4] * v.y + matrix.m[12];
        result.y = matrix.m[1] * v.x + matrix.m[5] * v.y + matrix.m[13];
        return result;
    }

    //////////////////////////////////////////////////////////////////////

    vec2d matrix_apply(vec2d const &v, gl_matrix const &matrix)
    {
        vec2d result;
        result.x = matrix.m[0] * v.x + matrix.m[4] * v.y + matrix.m[12];
        result.y = matrix.m[1] * v.x + matrix.m[5] * v.y + matrix.m[13];
        return result;
    }

}    // namespace gerber_3d
