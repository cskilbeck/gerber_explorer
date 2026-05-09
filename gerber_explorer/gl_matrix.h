#pragma once

#include "gerber_2d.h"

namespace gl
{
    struct alignas(16) matrix
    {
        float m[16];
    };

    matrix make_identity();
    matrix make_translate(float x, float y);
    matrix make_scale(float x, float y);
    matrix make_ortho(int w, int h);
    matrix make_ortho_vk(int w, int h);    // Vulkan/SDL_GPU: Y flipped
    matrix matrix_multiply(matrix const &a, matrix const &b);

    gerber_lib::vec2f matrix_apply(gerber_lib::vec2f const &v, matrix const &matrix);
    gerber_lib::vec2d matrix_apply(gerber_lib::vec2d const &v, matrix const &matrix);

}    // namespace gerber_3d
