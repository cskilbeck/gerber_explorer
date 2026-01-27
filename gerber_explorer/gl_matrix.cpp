#include "dx_math.h"

#include "gl_matrix.h"

namespace gl
{
    using namespace DirectX;
    using gerber_lib::vec2f;
    using gerber_lib::vec2d;

    // gl_matrix is alignas(16)
    inline XMMATRIX load_gl(const matrix& m) {
        return XMLoadFloat4x4A(reinterpret_cast<const XMFLOAT4X4A*>(m.m));
    }

    inline void store_gl(matrix& dest, FXMMATRIX src) {
        XMStoreFloat4x4A(reinterpret_cast<XMFLOAT4X4A*>(dest.m), src);
    }

    matrix make_identity()
    {
        matrix t;
        store_gl(t, XMMatrixIdentity());
        return t;
    }

    matrix make_translate(float x, float y)
    {
        matrix t;
        store_gl(t, XMMatrixTranslation(x, y, 0.0f));
        return t;
    }

    matrix make_scale(float x, float y)
    {
        matrix t;
        store_gl(t, XMMatrixScaling(x, y, 1.0f));
        return t;
    }

    matrix make_ortho(int w, int h)
    {
        matrix t;
        store_gl(t, XMMatrixSet(
            2.0f / w, 0.0f,     0.0f,  0.0f,
            0.0f,     2.0f / h, 0.0f,  0.0f,
            0.0f,     0.0f,    -1.0f,  0.0f,
           -1.0f,    -1.0f,     0.0f,  1.0f
        ));
        return t;
    }

    matrix matrix_multiply(matrix const &a, matrix const &b)
    {
        XMMATRIX mA = load_gl(a);
        XMMATRIX mB = load_gl(b);

        // OpenGL (Col-Major) A * B == DirectXMath (Row-Major) B * A
        matrix t;
        store_gl(t, XMMatrixMultiply(mB, mA));
        return t;
    }

    vec2f matrix_apply(vec2f const &v, matrix const &matrix)
    {
        XMMATRIX m = load_gl(matrix);
        XMVECTOR vector = XMVectorSet(v.x, v.y, 0.0f, 1.0f);
        XMVECTOR result = XMVector2Transform(vector, m);
        return { XMVectorGetX(result), XMVectorGetY(result) };
    }

    vec2d matrix_apply(vec2d const &v, matrix const &matrix)
    {
        XMMATRIX m = load_gl(matrix);
        XMVECTOR vector = XMVectorSet((float)v.x, (float)v.y, 0.0f, 1.0f);
        XMVECTOR result = XMVector2Transform(vector, m);
        return { (double)XMVectorGetX(result), (double)XMVectorGetY(result) };
    }
}
