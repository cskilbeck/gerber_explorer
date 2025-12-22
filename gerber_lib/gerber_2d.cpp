#include "math.h"

#include "gerber_2d.h"

namespace gerber_lib
{
    namespace gerber_2d
    {
        //////////////////////////////////////////////////////////////////////

        vec2d::vec2d(double x, double y) : x(x), y(y)
        {
        }

        //////////////////////////////////////////////////////////////////////

        vec2d::vec2d(double x, double y, matrix const &m) : x(x * m.A + y * m.C + m.X), y(x * m.B + y * m.D + m.Y)
        {
        }

        //////////////////////////////////////////////////////////////////////

        vec2d::vec2d(vec2d const &o, matrix const &m) : vec2d(o.x, o.y, m)
        {
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
        // make a matrix which maps window rect to world coordinates
        // if aspect ratio(view_rect) != aspect_ratio(window_rect), there will be distortion

        matrix matrix::world_to_window_transform(rect const &window, rect const &view)
        {
            matrix origin = matrix::translate(view.min_pos.negate());
            matrix scale = matrix::scale({ window.width() / view.width(), window.height() / view.height() });
            matrix flip = matrix::scale({ 1, -1 });
            matrix offset = matrix::translate({ 0, window.height() });

            matrix m = matrix::identity();
            m = matrix::multiply(m, origin);
            m = matrix::multiply(m, scale);
            m = matrix::multiply(m, flip);
            m = matrix::multiply(m, offset);
            return m;
        }

        //////////////////////////////////////////////////////////////////////

        vec2d transform_point(matrix const &m, vec2d const &p)
        {
            return vec2d(p.x, p.y, m);
        }

        //////////////////////////////////////////////////////////////////////

        void get_arc_extents(vec2d const &center, double radius, double start_degrees, double end_degrees, rect &extent)
        {
            // get the endpoints of the arc

            auto arc_point = [&](double d) {
                double radians = deg_2_rad(d);
                return vec2d{ center.x + cos(radians) * radius, center.y + sin(radians) * radius };
            };

            vec2d start_point = arc_point(start_degrees);
            vec2d end_point = arc_point(end_degrees);

            // initial extents are just the start and end points

            extent.min_pos = { std::min(start_point.x, end_point.x), std::min(start_point.y, end_point.y) };
            extent.max_pos = { std::max(start_point.x, end_point.x), std::max(start_point.y, end_point.y) };

            // check if the arc goes through any cardinal points

            bool full_circle = fabs(end_degrees - start_degrees) >= 360;

            double s = fmod(start_degrees, 360.0);
            if(s < 0) {
                s += 360.0;
            }
            double e = fmod(end_degrees, 360.0);
            if(e < 0) {
                e += 360.0;
            }

            auto crosses_cardinal = [&](double d) {
                if(full_circle) {
                    return true;
                }
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


    }    // namespace gerber_2d

}    // namespace gerber_lib
