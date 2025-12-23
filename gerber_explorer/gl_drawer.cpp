//////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "tesselator.h"

#include "gerber_lib.h"
#include "log_drawer.h"
#include "gl_drawer.h"

LOG_CONTEXT("gl_drawer", debug);

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    bool gl_drawer::is_clockwise(std::vector<vec2f> const &points, size_t start, size_t end)
    {
        double sum = 0;
        for(size_t i = start, n = end - 1; i != end; n = i++) {
            vec2f const &p1 = points[i];
            vec2f const &p2 = points[n];
            sum += (p2.x - p1.x) * (p2.y + p1.y);
        }
        return sum < 0;    // Negative sum indicates clockwise orientation
    }
    //////////////////////////////////////////////////////////////////////

    void gl_drawer::set_gerber(gerber_lib::gerber *g)
    {
        gerber_file = g;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::on_finished_loading()
    {
        tess = tessNewTess(nullptr);
        tessSetOption(tess, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);

        points.clear();

        gerber_file->draw(*this);

        vertex_array.init(*program, vertices.size());
        vertex_array.activate();
        void *v;
        GL_CHECK(v = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(vec2f) * vertices.size(), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        memcpy(v, vertices.data(), vertices.size() * sizeof(vec2f));
        GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));

        index_array.init((int)indices.size());
        index_array.activate();
        void *i;
        GL_CHECK(i= glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(uint32_t) * indices.size(), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        memcpy(i, indices.data(), indices.size() * sizeof(uint32_t));
        GL_CHECK(glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER));
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id)
    {
        // LOG_DEBUG("ENTITY ID {} HAS {} elements, polarity is {}", entity_id, num_elements, polarity);

        double constexpr ARC_DEGREES = 3.6;

        draw_call_flags flag = polarity == gerber_lib::polarity_clear ? draw_call_flag_clear : draw_call_flag_none;

        base_vert = (int)points.size();

        current_flag = flag;

        // add a point to the list if it's more than ## away from the previous point

        auto add_arc_point = [this](gerber_lib::gerber_draw_element const &element, double t) {
            double radians = gerber_lib::deg_2_rad(t);
            double x = cos(radians) * element.arc.radius + element.arc.center.x;
            double y = sin(radians) * element.arc.radius + element.arc.center.y;
            points.emplace_back(x, y);
        };

        // create array of points, approximating arcs

        for(size_t n = 0; n < num_elements; ++n) {

            gerber_lib::gerber_draw_element const &element = elements[n];

            switch(element.draw_element_type) {

            case gerber_lib::draw_element_line:
                points.emplace_back(element.line.start.x, element.line.start.y);
                points.emplace_back(element.line.end.x, element.line.end.y);
                break;

            case gerber_lib::draw_element_arc: {

                double start = element.arc.start_degrees;
                double end = element.arc.end_degrees;
                if(fabs(start - end) >= ARC_DEGREES) {
                    double final_angle = start;
                    if(start < end) {
                        for(double t = start; t < end; t += ARC_DEGREES) {
                            add_arc_point(element, t);
                            final_angle = t;
                        }
                    } else {
                        for(double t = start; t > end; t -= ARC_DEGREES) {
                            add_arc_point(element, t);
                            final_angle = t;
                        }
                    }
                    if(final_angle != end) {
                        add_arc_point(element, end);
                    }
                }
            } break;
            }
        }

        if(points.size() < 3) {
            LOG_WARNING("CULLED SECTION OF ENTITY {}", entity_id);
            return;
        }

        // force clockwise ordering for this element
        if(!is_clockwise(points, base_vert, points.size())) {
            std::reverse(points.begin() + base_vert, points.end());
        }
        tessAddContour(tess, 2, points.data() + base_vert, sizeof(vec2f), static_cast<int>(points.size() - base_vert));

        current_entity_id = entity_id;
    }

    void gl_drawer::finish_element(int entity_id)
    {
        if(points.empty()) {
            return;
        }
        // Tesselate to get the outline
        tessTesselate(tess, TESS_WINDING_POSITIVE, TESS_BOUNDARY_CONTOURS, 0, 2, nullptr);
        const float *verts = tessGetVertices(tess);
        const int *elems = tessGetElements(tess);
        const int nelems = tessGetElementCount(tess);

        // Now, we could snag some indices here for drawing the outline...

        // Tesselate to get the triangles
        for(int i = 0; i < nelems; ++i) {
            int b = elems[i * 2];
            int n = elems[i * 2 + 1];
            tessAddContour(tess, 2, &verts[b * 2], sizeof(float) * 2, n);
        }
        tessTesselate(tess, TESS_WINDING_POSITIVE, TESS_POLYGONS, 3, 2, nullptr);

        float const *tri_verts = tessGetVertices(tess);
        int const tri_nverts = tessGetVertexCount(tess);

        int const *tri_elems = tessGetElements(tess);
        int const tri_nelems = tessGetElementCount(tess);

        size_t base = vertices.size();
        for(int v = 0; v < tri_nverts; ++v) {
            float const *vrt = tri_verts + v * 2;
            vertices.emplace_back(vrt[0], vrt[1]);
        }

        uint32_t index_base = indices.size();

        for(int x = 0; x < tri_nelems; ++x) {
            int const *p = &tri_elems[x * 3];
            int const p0 = p[0];
            int const p1 = p[1];
            int const p2 = p[2];
            if(p0 != TESS_UNDEF && p1 != TESS_UNDEF && p2 != TESS_UNDEF) {
                indices.push_back(p0 + base);
                indices.push_back(p1 + base);
                indices.push_back(p2 + base);
            }
        }
        draw_calls.emplace_back(index_base, indices.size() - index_base);
        points.clear();
    }


    void gl_drawer::render(uint32_t color)
    {
        program->set_color(color);
        vertex_array.activate();
        index_array.activate();
        for(auto const &s : draw_calls) {
            GL_CHECK(glDrawElements(GL_TRIANGLES, s.length, GL_UNSIGNED_INT, (void *)(s.start * sizeof(GLuint))));
        }
    }

    void gl_drawer::render_triangle(int draw_call_index, int triangle_index, uint32_t color)
    {
        program->set_color(color);
        vertex_array.activate();
        index_array.activate();
        if(draw_call_index < 0) {
            draw_call_index = 0;
        } else if(draw_call_index >= draw_calls.size()) {
            draw_call_index = draw_calls.size() - 1;
        }
        vertex_span &span = draw_calls[draw_call_index];
        int num_triangles = span.length / 3;
        if(triangle_index < 0) {
            triangle_index = 0;
        } else if(triangle_index >= num_triangles) {
            triangle_index = num_triangles - 1;
        }
        GL_CHECK(glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void *)(span.start + triangle_index * 3 * sizeof(GLuint))));
    }
}    // namespace gerber_3d
