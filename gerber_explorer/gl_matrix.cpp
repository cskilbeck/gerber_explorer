#include "gl_matrix.h"
#include <string.h>

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    void make_ortho(gl_matrix mat, int w, int h)
    {
        mat[0] = 2.0f / w;
        mat[1] = 0.0f;
        mat[2] = 0.0f;
        mat[3] = -1.0f;

        mat[4] = 0.0f;
        mat[5] = 2.0f / h;
        mat[6] = 0.0f;
        mat[7] = -1.0f;

        mat[8] = 0.0f;
        mat[9] = 0.0f;
        mat[10] = -1.0f;
        mat[11] = 0.0f;

        mat[12] = 0.0f;
        mat[13] = 0.0f;
        mat[14] = 0.0f;
        mat[15] = 1.0f;
    }

    //////////////////////////////////////////////////////////////////////

    void make_translate(gl_matrix mat, float x, float y)
    {
        mat[0] = 1.0f;
        mat[1] = 0.0f;
        mat[2] = 0.0f;
        mat[3] = x;

        mat[4] = 0.0f;
        mat[5] = 1.0f;
        mat[6] = 0.0f;
        mat[7] = y;

        mat[8] = 0.0f;
        mat[9] = 0.0f;
        mat[10] = 1.0f;
        mat[11] = 0.0f;

        mat[12] = 0.0f;
        mat[13] = 0.0f;
        mat[14] = 0.0f;
        mat[15] = 1.0f;
    }

    //////////////////////////////////////////////////////////////////////

    void make_scale(gl_matrix mat, float x_scale, float y_scale)
    {
        mat[0] = x_scale;
        mat[1] = 0.0f;
        mat[2] = 0.0f;
        mat[3] = 0.0f;

        mat[4] = 0.0f;
        mat[5] = y_scale;
        mat[6] = 0.0f;
        mat[7] = 0.0f;

        mat[8] = 0.0f;
        mat[9] = 0.0f;
        mat[10] = 1.0f;
        mat[11] = 0.0f;

        mat[12] = 0.0f;
        mat[13] = 0.0f;
        mat[14] = 0.0f;
        mat[15] = 1.0f;
    }

    //////////////////////////////////////////////////////////////////////

    void matrix_multiply(gl_matrix const a, gl_matrix const b, gl_matrix out)
    {
        gl_matrix t;
        for(int i = 0; i < 16; i += 4) {
            t[i + 0] = a[i] * b[0] + a[i + 1] * b[4] + a[i + 2] * b[8] + a[i + 3] * b[12];
            t[i + 1] = a[i] * b[1] + a[i + 1] * b[5] + a[i + 2] * b[9] + a[i + 3] * b[13];
            t[i + 2] = a[i] * b[2] + a[i + 1] * b[6] + a[i + 2] * b[10] + a[i + 3] * b[14];
            t[i + 3] = a[i] * b[3] + a[i + 1] * b[7] + a[i + 2] * b[11] + a[i + 3] * b[15];
        }
        memcpy(out, t, sizeof(gl_matrix));
    }

}    // namespace gerber_3d