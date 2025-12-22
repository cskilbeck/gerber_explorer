#pragma once

// 0xAABBGGRR (RGBA in memory)

namespace gl_color
{
    struct float4
    {
        float f[4];

        float4() = default;

        float4(float *v)
        {
            memcpy(f, v, sizeof(float) * 4);
        }

        float4(uint32_t color)
        {
            float constexpr s = 1.0f / 255.0f;
            f[0] = (color & 0xff) * s;
            f[1] = ((color >> 8) & 0xff) * s;
            f[2] = ((color >> 16) & 0xff) * s;
            f[3] = (color >> 24) * s;
        }

        float4(float x, float y, float z, float w)
        {
            f[0] = x;
            f[1] = y;
            f[2] = z;
            f[3] = w;
        }

        float4 &operator=(float *v)
        {
            return *this = v;
        }

        float4 &operator=(uint32_t color)
        {
            return *this = float4(color);
        }

        operator float *()
        {
            return f;
        }

        operator float const *() const
        {
            return f;
        }

        explicit operator uint32_t() const
        {
            uint32_t red = (int)(f[0] * 255.0f) & 0xff;
            uint32_t green = (int)(f[1] * 255.0f) & 0xff;
            uint32_t blue = (int)(f[2] * 255.0f) & 0xff;
            uint32_t alpha = (int)(f[3] * 255.0f) & 0xff;
            return (alpha << 24) | (blue << 16) | (green << 8) | red;
        }

        float const &operator[](int i) const
        {
            return f[i];
        }

        float &operator[](int i)
        {
            return f[i];
        }
    };

    inline uint32_t from_floats(float4 const &f)
    {
        return (uint32_t)f;
    }

    inline float4 to_floats(uint32_t color)
    {
        return float4(color);
    }

    enum : uint32_t
    {
        clear = 0x00000000,
        black = 0xff000000,
        white = 0xffffffff,
        red = 0xff0000ff,
        dim_green = 0xff008000,
        green = 0xff00ff00,
        blue = 0xffff0000,
        yellow = 0xff00ffff,
        cyan = 0xffffff00,
        magenta = 0xffff00ff,
        silver = 0xffc0c0c0,
        gray = 0xff808080,
        maroon = 0xff000080,
        olive = 0xff008080,
        purple = 0xff800080,
        teal = 0xff808000,
        navy = 0xff800000,
        dark_red = 0xff00008b,
        brown = 0xff2a2aa5,
        firebrick = 0xff2222b2,
        crimson = 0xff3c14dc,
        tomato = 0xff4763ff,
        coral = 0xff507fff,
        indian_red = 0xff5c5ccd,
        light_coral = 0xff8080f0,
        dark_salmon = 0xff7a96e9,
        salmon = 0xff7280fa,
        light_salmon = 0xff7aa0ff,
        orange_red = 0xff0045ff,
        dark_orange = 0xff008cff,
        orange = 0xff00a5ff,
        gold = 0xff00d7ff,
        dark_golden_rod = 0xff0b86b8,
        golden_rod = 0xff20a5da,
        pale_golden_rod = 0xffaae8ee,
        dark_khaki = 0xff6bb7bd,
        khaki = 0xff8ce6f0,
        yellow_green = 0xff32cd9a,
        dark_olive_green = 0xff2f6b55,
        olive_drab = 0xff238e6b,
        lawn_green = 0xff00fc7c,
        chartreuse = 0xff00ff7f,
        green_yellow = 0xff2fffad,
        dark_green = 0xff006400,
        forest_green = 0xff228b22,
        lime_green = 0xff32cd32,
        light_green = 0xff90ee90,
        pale_green = 0xff98fb98,
        dark_sea_green = 0xff8fbc8f,
        medium_spring_green = 0xff9afa00,
        spring_green = 0xff7fff00,
        sea_green = 0xff578b2e,
        medium_aqua_marine = 0xffaacd66,
        medium_sea_green = 0xff71b33c,
        light_sea_green = 0xffaab220,
        dark_slate_gray = 0xff4f4f2f,
        dark_cyan = 0xff8b8b00,
        aqua = 0xffffff00,
        dark_turquoise = 0xffd1ce00,
        turquoise = 0xffd0e040,
        medium_turquoise = 0xffccd148,
        pale_turquoise = 0xffeeeeaf,
        aqua_marine = 0xffd4ff7f,
        powder_blue = 0xffe6e0b0,
        cadet_blue = 0xffa09e5f,
        steel_blue = 0xffb48246,
        corn_flower_blue = 0xffed9564,
        deep_sky_blue = 0xffffbf00,
        dodger_blue = 0xffff901e,
        light_blue = 0xffe6d8ad,
        sky_blue = 0xffebce87,
        light_sky_blue = 0xffface87,
        midnight_blue = 0xff701919,
        dark_blue = 0xff8b0000,
        royal_blue = 0xffe16941,
        blue_violet = 0xffe22b8a,
        dark_slate_blue = 0xff8b3d48,
        slate_blue = 0xffcd5a6a,
        medium_slate_blue = 0xffee687b,
        medium_purple = 0xffdb7093,
        dark_magenta = 0xff8b008b,
        dark_violet = 0xffd30094,
        dark_orchid = 0xffcc3299,
        medium_orchid = 0xffd355ba,
        thistle = 0xffd8bfd8,
        plum = 0xffdda0dd,
        violet = 0xffee82ee,
        medium_violet_red = 0xff8515c7,
        pale_violet_red = 0xff9370db,
        deep_pink = 0xff9314ff,
        hot_pink = 0xffb469ff,
        light_pink = 0xffc1b6ff,
        pink = 0xffcbc0ff,
        antique_white = 0xffd7ebfa,
        beige = 0xffdcf5f5,
        bisque = 0xffc4e4ff,
        blanched_almond = 0xffcdebff,
        wheat = 0xffb3def5,
        corn_silk = 0xffdcf8ff,
        lemon_chiffon = 0xffcdfaff,
        light_golden_rod_yellow = 0xffd2fafa,
        light_yellow = 0xffe0ffff,
        saddle_brown = 0xff13458b,
        sienna = 0xff2d52a0,
        chocolate = 0xff1e69d2,
        peru = 0xff3f85cd,
        sandy_brown = 0xff60a4f4,
        burly_wood = 0xff87b8de,
        tan = 0xff8cb4d2,
        rosy_brown = 0xff8f8fbc,
        moccasin = 0xffb5e4ff,
        navajo_white = 0xffaddeff,
        peach_puff = 0xffb9daff,
        misty_rose = 0xffe1e4ff,
        lavender_blush = 0xfff5f0ff,
        linen = 0xffe6f0fa,
        old_lace = 0xffe6f5fd,
        papaya_whip = 0xffd5efff,
        sea_shell = 0xffeef5ff,
        mint_cream = 0xfffafff5,
        slate_gray = 0xff908070,
        light_slate_gray = 0xff998877,
        light_steel_blue = 0xffdec4b0,
        lavender = 0xfffae6e6,
        floral_white = 0xfff0faff,
        alice_blue = 0xfffff8f0,
        ghost_white = 0xfffff8f8,
        honeydew = 0xfff0fff0,
        ivory = 0xfff0ffff,
        azure = 0xfffffff0,
        snow = 0xfffafaff,
        dim_grey = 0xff696969,
        grey = 0xff808080,
        dark_gray = 0xffa9a9a9,
        light_grey = 0xffd3d3d3,
        gainsboro = 0xffdcdcdc,
        white_smoke = 0xfff5f5f5,
    };

}    // namespace color