#include "gerber_2d.h"

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    vec2d::vec2d(double x, double y, matrix const &transform)
        : x(x * transform.A + y * transform.C + transform.X), y(x * transform.B + y * transform.D + transform.Y)
    {
    }

    //////////////////////////////////////////////////////////////////////

    std::string rect::to_string() const
    {
        return std::format("(MIN:{} MAX:{})", min_pos, max_pos);
    }

    //////////////////////////////////////////////////////////////////////

    matrix matrix::multiply(matrix const &l, matrix const &r)
    {
        return matrix(l.A * r.A + l.B * r.C,          //
                      l.A * r.B + l.B * r.D,          //
                      l.C * r.A + l.D * r.C,          //
                      l.C * r.B + l.D * r.D,          //
                      l.X * r.A + l.Y * r.C + r.X,    //
                      l.X * r.B + l.Y * r.D + r.Y);
    }

    //////////////////////////////////////////////////////////////////////

    matrix matrix::identity()
    {
        return matrix(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    }

    //////////////////////////////////////////////////////////////////////

    matrix matrix::translate(vec2d const &offset)
    {
        return matrix(1.0, 0.0, 0.0, 1.0, offset.x, offset.y);
    }

    //////////////////////////////////////////////////////////////////////

    matrix matrix::rotate(double angle_degrees)
    {
        double radians = deg_2_rad(angle_degrees);
        double s = sin(radians);
        double c = cos(radians);
        return matrix(c, s, -s, c, 0.0, 0.0);
    }

    //////////////////////////////////////////////////////////////////////

    matrix matrix::scale(vec2d const &scale)
    {
        return matrix(scale.x, 0.0, 0.0, scale.y, 0.0, 0.0);
    }

    //////////////////////////////////////////////////////////////////////

    matrix matrix::rotate_around(double angle_degrees, vec2d const &pos)
    {
        matrix m;
        m = translate({ -pos.x, -pos.y });
        m = multiply(rotate(angle_degrees), m);
        m = multiply(translate(pos), m);
        return m;
    }

    //////////////////////////////////////////////////////////////////////

    matrix matrix::invert(matrix const &m)
    {
        double det = m.A * m.D - m.B * m.C;

        // Check for singular matrix
        if(det == 0) {
            return identity();
        }

        return matrix(m.D / det, -m.B / det, -m.C / det, m.A / det, (m.B * m.Y - m.D * m.X) / det, (m.C * m.X - m.A * m.Y) / det);
    }

    //////////////////////////////////////////////////////////////////////

    vec2d transform_point(matrix const &m, vec2d const &p)
    {
        return vec2d(p.x, p.y, m);
    }

    //////////////////////////////////////////////////////////////////////

    rect rect::normalize() const
    {
        double x1 = std::min(min_pos.x, max_pos.x);
        double y1 = std::min(min_pos.y, max_pos.y);
        double x2 = std::max(min_pos.x, max_pos.x);
        double y2 = std::max(min_pos.y, max_pos.y);
        return { { x1, y1 }, { x2, y2 } };
    }

    //////////////////////////////////////////////////////////////////////

    rect rect::union_with(rect const &o) const
    {
        double x1 = std::min(min_pos.x, o.min_pos.x);
        double y1 = std::min(min_pos.y, o.min_pos.y);
        double x2 = std::max(max_pos.x, o.max_pos.x);
        double y2 = std::max(max_pos.y, o.max_pos.y);
        return { { x1, y1 }, { x2, y2 } };
    }

    //////////////////////////////////////////////////////////////////////

    rect get_arc_extents(vec2d const &center, double radius, double start_degrees, double end_degrees)
    {
        rect extent;

        // if it's a full circle, the extent is just a box with 2*radius sides
        if(fabs(end_degrees - start_degrees) >= 360) {
            vec2d r{ radius, radius };
            extent.min_pos = center.subtract(r);
            extent.max_pos = center.add(r);
        } else {

            // get the endpoints of the arc
            auto arc_point = [&](double d) {
                double radians = deg_2_rad(d);
                return vec2d{ center.x + cos(radians) * radius, center.y + sin(radians) * radius };
            };

            vec2d start_point = arc_point(start_degrees);
            vec2d end_point = arc_point(end_degrees);

            // initial extents are the start and end points
            extent.min_pos = { std::min(start_point.x, end_point.x), std::min(start_point.y, end_point.y) };
            extent.max_pos = { std::max(start_point.x, end_point.x), std::max(start_point.y, end_point.y) };

            // normalize start, end angles
            double s = fmod(start_degrees, 360.0);
            if(s < 0) {
                s += 360.0;
            }
            double e = fmod(end_degrees, 360.0);
            if(e < 0) {
                e += 360.0;
            }

            // check if the arc goes through any cardinal points
            auto crosses_cardinal = [&](double d) {
                if(s <= e) {
                    return s <= d && d < e;
                }
                return s <= d || d < e;
            };

            if(crosses_cardinal(0)) {
                extent.max_pos.x = center.x + radius;
            }
            if(crosses_cardinal(90)) {
                extent.max_pos.y = center.y + radius;
            }
            if(crosses_cardinal(180)) {
                extent.min_pos.x = center.x - radius;
            }
            if(crosses_cardinal(270)) {
                extent.min_pos.y = center.y - radius;
            }
        }
        return extent;
    }

    //////////////////////////////////////////////////////////////////////

    bool line_intersects_rect(rect const &r, vec2f const &p1, vec2f const &p2)
    {
        double line_min_x = p1.x < p2.x ? p1.x : p2.x;
        double line_max_x = p1.x > p2.x ? p1.x : p2.x;
        if(line_max_x < r.min_pos.x || line_min_x > r.max_pos.x) {
            return false;
        }

        double line_min_y = p1.y < p2.y ? p1.y : p2.y;
        double line_max_y = p1.y > p2.y ? p1.y : p2.y;
        if(line_max_y < r.min_pos.y || line_min_y > r.max_pos.y) {
            return false;
        }

        double tmin = 0.0;
        double tmax = 1.0;
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;

        auto clip = [&tmax, &tmin](double p, double q) {
            if(p == 0) {
                return q >= 0;
            }
            double t = q / p;
            if(p < 0) {
                if(t > tmax) {
                    return false;
                }
                if(t > tmin) {
                    tmin = t;
                }
            } else {
                if(t < tmin) {
                    return false;
                }
                if(t < tmax) {
                    tmax = t;
                }
            }
            return true;
        };

        if(!clip(-dx, p1.x - r.min_pos.x)) {
            return false;
        }
        if(!clip(dx, r.max_pos.x - p1.x)) {
            return false;
        }
        if(!clip(-dy, p1.y - r.min_pos.y)) {
            return false;
        }
        if(!clip(dy, r.max_pos.y - p1.y)) {
            return false;
        }

        return tmin <= tmax;
    }

    //////////////////////////////////////////////////////////////////////

    bool point_in_poly(vec2f *points, int num_points, vec2f p)
    {
        bool inside = false;
        for(size_t i = 0, j = num_points - 1; i < num_points; j = i++) {
            if(((points[i].y > p.y) != (points[j].y > p.y)) &&
               (p.x < (points[j].x - points[i].x) * (p.y - points[i].y) / (points[j].y - points[i].y) + points[i].x)) {
                inside = !inside;
            }
        }
        return inside;
    }

}    // namespace gerber_lib
