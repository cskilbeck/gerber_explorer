#pragma once

#include <cmath>

static constexpr double PI = 3.1415926535897932384626433;

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    inline double round_precise(double x, int precision)
    {
        double p = std::pow(10.0, precision);
        return round(x * p) / p;
    }

    //////////////////////////////////////////////////////////////////////

    inline double deg_2_rad(double x)
    {
        return x * (PI / 180.0);
    }

    //////////////////////////////////////////////////////////////////////

    inline float deg_2_radf(float x)
    {
        return (float)(x * (PI / 180.0));
    }

    //////////////////////////////////////////////////////////////////////

    inline float deg_2_radf(double x)
    {
        return (float)(x * (PI / 180.0));
    }

    //////////////////////////////////////////////////////////////////////

    inline double rad_2_deg(double x)
    {
        return x * (180 / PI);
    }

}    // namespace gerber_lib
