//////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "tesselator.h"

#include "gerber_lib.h"
#include "log_drawer.h"
#include "gl_drawer.h"

LOG_CONTEXT("gl_drawer", info);

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    using namespace gerber_lib;

    bool is_clockwise(std::vector<gerber_2d::vec2f> const &points, size_t start, size_t end)
    {
        double sum = 0;
        for(size_t i = start, n = end - 1; i != end; n = i++) {
            gerber_2d::vec2f const &p1 = points[i];
            gerber_2d::vec2f const &p2 = points[n];
            sum += (p2.x - p1.x) * (p2.y + p1.y);
        }
        return sum < 0;    // Negative sum indicates clockwise orientation
    }

    //////////////////////////////////////////////////////////////////////

    void gl_tesselator::clear()
    {
        LOG_DEBUG("BOUNDARY TESSELATOR: Clear");
        entities.clear();      // the entities (each may consist of multiple draw calls (if a macro has disconnected primitives))
        vertices.clear();      // the verts (for outlines and fills)
        indices.clear();       // the indices (for fills)
        boundaries.clear();    // spans for drawing outlines (GL_TRIANGLES)
        fills.clear();         // spans for drawing fills (GL_LINE_LOOP)
    }

    //////////////////////////////////////////////////////////////////////

    void gl_tesselator::new_entity(int entity_id, draw_call_flags flags)
    {
        LOG_DEBUG("BOUNDARY New Entity: {}", entity_id);
        finish_entity();

        entities.emplace_back(entity_id, (int)boundaries.size(), (int)fills.size(), 0, 0, flags);
        boundary_stesselator = tessNewTess(nullptr);
        tessSetOption(boundary_stesselator, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);
        tessSetOption(boundary_stesselator, TESS_REVERSE_CONTOURS, 1);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_tesselator::append_points(size_t offset)
    {
        tesselator_entity &e = entities.back();
        if(e.bounds.is_empty_rect()) {
            e.bounds.min_pos = e.bounds.max_pos = gerber_2d::vec2d(points[offset]);
        }
        for(size_t i=offset; i < points.size(); ++i) {
            e.bounds.expand_to_contain(gerber_2d::vec2d(points[i]));
        }
        tessAddContour(boundary_stesselator, 2, points.data() + offset, sizeof(float) * 2, points.size() - offset);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_tesselator::finish_entity()
    {
        if(boundary_stesselator != nullptr) {
            tessTesselate(boundary_stesselator, TESS_WINDING_POSITIVE, TESS_BOUNDARY_CONTOURS, 0, 2, nullptr);

            const float *verts = tessGetVertices(boundary_stesselator);
            const int *elems = tessGetElements(boundary_stesselator);
            const int nelems = tessGetElementCount(boundary_stesselator);

            // New tesselator for the interior
            TESStesselator *interior_tesselator = tessNewTess(nullptr);
            tessSetOption(interior_tesselator, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);
            tessSetOption(interior_tesselator, TESS_REVERSE_CONTOURS, 1);

            uint32_t boundary_base = boundaries.size();

            for(int i = 0; i < nelems; ++i) {
                int outline_base = vertices.size();
                int b = elems[i * 2];
                int n = elems[i * 2 + 1];
                float const *f = &verts[b * 2];
                tessAddContour(interior_tesselator, 2, f, sizeof(float) * 2, n);
                for(int p = 0; p < n; ++p) {
                    vertices.emplace_back(f[0], f[1]);
                    f += 2;
                }
                boundaries.emplace_back(outline_base, vertices.size() - outline_base);
            }

            tessTesselate(interior_tesselator, TESS_WINDING_POSITIVE, TESS_POLYGONS, 3, 2, nullptr);

            float const *tri_verts = tessGetVertices(interior_tesselator);
            int const tri_nverts = tessGetVertexCount(interior_tesselator);
            int const *tri_elems = tessGetElements(interior_tesselator);
            int const tri_nelems = tessGetElementCount(interior_tesselator);

            size_t base = vertices.size();
            for(int v = 0; v < tri_nverts; ++v) {
                float const *vrt = tri_verts + v * 2;
                vertices.emplace_back(vrt[0], vrt[1]);
            }

            uint32_t index_base = indices.size();

            uint32_t fill_base = fills.size();

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
            fills.emplace_back(index_base, indices.size() - index_base);

            tesselator_entity &e = entities.back();
            e.num_outlines = boundaries.size() - boundary_base;
            e.num_fills = fills.size() - fill_base;

            tessDeleteTess(interior_tesselator);

            tessDeleteTess(boundary_stesselator);
            boundary_stesselator = nullptr;

            points.clear();
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_tesselator::finalize()
    {
        finish_entity();
    }

    //////////////////////////////////////////////////////////////////////
    // This may run in a thread, so vertex/index buffers cannot be
    // initialized here - do that in on_finished_loading()

    void gl_drawer::set_gerber(gerber *g)
    {
        gerber_file = g;
        current_entity_id = -1;
        tesselator.clear();
        g->draw(*this);
        tesselator.new_entity(current_entity_id, current_flag);
        tesselator.finalize();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::on_finished_loading()
    {
        vertex_array.init(*program, tesselator.vertices.size());
        vertex_array.activate();
        update_buffer<GL_ARRAY_BUFFER>(tesselator.vertices);

        index_array.init(tesselator.indices.size());
        index_array.activate();
        update_buffer<GL_ELEMENT_ARRAY_BUFFER>(tesselator.indices);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        using gerber_2d::vec2d;
        using gerber_lib::gerber_draw_element;

        double constexpr THRESHOLD = 1e-38;
        double constexpr ARC_DEGREES = 3.6;

        draw_call_flags flag = polarity == polarity_clear ? draw_call_flag_clear : draw_call_flag_none;

        if(entity_id != current_entity_id) {
            tesselator.new_entity(entity_id, flag);
        }

        current_flag = flag;
        current_entity_id = entity_id;

        std::vector<vec2f> &points = tesselator.points;
        size_t offset = points.size();

        auto add_point = [&](double x, double y) {
            if(points.empty() || fabs(points.back().x - x) > THRESHOLD || fabs(points.back().y - y) > THRESHOLD) {
                points.emplace_back(x, y);
            }
        };

        auto add_arc_point = [&](gerber_draw_element const &element, double t) {
            double radians = deg_2_rad(t);
            double x = cos(radians) * element.arc.radius + element.arc.center.x;
            double y = sin(radians) * element.arc.radius + element.arc.center.y;
            add_point(x, y);
        };
        for(size_t n = 0; n < num_elements; ++n) {

            gerber_draw_element const &element = elements[n];

            switch(element.draw_element_type) {

            case draw_element_line:
                add_point(element.line.end.x, element.line.end.y);
                break;

            case draw_element_arc: {

                double start = element.arc.start_degrees;
                double end = element.arc.end_degrees;

                double final_angle = end;

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
            } break;
            }
        }

        if(points.size() < 3) {
            LOG_WARNING("CULLED SECTION OF ENTITY {}", entity_id);
            return;
        }

        // if last point == first point, bin it
        if(points.back().x == points.front().x && points.back().y == points.front().y) {
            points.pop_back();
        }

        // force counter clockwise ordering

        if(is_clockwise(points, offset, points.size())) {
            // LOG_DEBUG("REVERSING entity {} from {} to {}", entity_id, offset, points.size());
            std::reverse(points.begin() + offset, points.end());
        }
        tesselator.append_points(offset);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::draw(bool fill, bool outline, bool wireframe, float outline_thickness)
    {
        program->use();

        vertex_array.activate();
        index_array.activate();

        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_BLEND);

        uint32_t constexpr fill_color = 0xff0000ff;
        uint32_t constexpr clear_color = 0xff00ff00;
        uint32_t constexpr outline_color = 0xffff0000;

        program->set_color(fill_color);

        for(auto const &e : tesselator.entities) {
            if(fill) {
                if(wireframe) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glLineWidth(1.0f);
                } else {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }

                if((e.flags & draw_call_flag_clear) != 0) {
                    program->set_color(clear_color);
                } else {
                    program->set_color(fill_color);
                }
                int end = e.num_fills + e.first_fill;
                for(int i = e.first_fill; i < end; ++i) {
                    tesselator_span const &s = tesselator.fills[i];
                    glDrawElements(GL_TRIANGLES, s.length, GL_UNSIGNED_INT, (void *)(s.start * sizeof(GLuint)));
                }
            }
            if(outline) {
                glLineWidth(outline_thickness);
                program->set_color(outline_color);
                int end = e.num_outlines + e.first_outline;
                for(int i = e.first_outline; i < end; ++i) {
                    tesselator_span const &s = tesselator.boundaries[i];
                    glDrawArrays(GL_LINE_LOOP, s.start, s.length);
                }
            }
        }
    }

}    // namespace gerber_3d
