#include "gl_matrix.h"

using namespace  gerber_lib::gerber_2d;

namespace gerber_3d
{
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
        t.m[5] = -2.0f / h;
        t.m[10] = -1.0f;
        t.m[12] = -1.0f;
        t.m[13] = 1.0f;
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
    // Make a transform matrix which optionally flips horizontally/vertically around cx,cy
    // And then places view_rect in the window
    // It's assumed that the aspect ratio of screen_w, screen_h == aspect ratio (view_rect)

    gl_matrix make_2d_transform(int screen_w, int screen_h, const rect &view_rect, vec2f center, bool flip_x, bool flip_y)
    {
        float sx = (float)(screen_w / view_rect.width());
        float sy = (float)(screen_h / view_rect.height());
        float fx = flip_x ? -1.0f : 1.0f;
        float fy = flip_y ? -1.0f : 1.0f;
        gl_matrix flip_m = make_identity();
        flip_m.m[0] = fx;
        flip_m.m[5] = fy;
        flip_m.m[12] = center.x - fx * center.x;
        flip_m.m[13] = center.y - fy * center.y;
        gl_matrix view_m = make_identity();
        view_m.m[0] = sx;
        view_m.m[5] = sy;
        view_m.m[12] = (float)(-(view_rect.min_pos.x * sx));
        view_m.m[13] = (float)(-(view_rect.min_pos.y * sy));
        gl_matrix ortho_m = make_ortho(screen_w, screen_h);
        gl_matrix temp = matrix_multiply(view_m, flip_m);
        return matrix_multiply(ortho_m, temp);
    }
}    // namespace gerber_3d
