//////////////////////////////////////////////////////////////////////

#include <glad/glad.h>

#include "tesselator.h"

#include "gerber_lib.h"
#include "log_drawer.h"
#include "gl_drawer.h"

#include "gl_colors.h"

LOG_CONTEXT("gl_drawer", debug);

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    struct gl_matrix;
    using namespace gerber_lib;

    bool is_clockwise(std::vector<vec2f> const &points, size_t start, size_t end)
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

    void gl_drawer::flag_touching_entities(rect const &world_rect, int clear_flags, int set_flags)
    {
        for(auto &e : entities) {
            e.flags &= ~clear_flags;
            bool hit = world_rect.contains_rect(e.bounds);
            if(!hit) {
                // if any corner is inside, it's a hit
                vec2f bl(world_rect.min_pos);
                vec2f tr(world_rect.max_pos);
                vec2f tl{ bl.x, tr.y };
                vec2f br{ tr.x, bl.y };
                auto p = outline_vertices.data() + e.outline_offset;
                int s = e.outline_size;
                hit = point_in_poly(p, s, bl) || point_in_poly(p, s, tr) || point_in_poly(p, s, tl) || point_in_poly(p, s, br);
            }
            // else do more expensive check (if bounding rects overlap)
            if(!hit && world_rect.overlaps_rect(e.bounds)) {
                int s = e.outline_offset;
                int end = s + e.outline_size - 1;
                int t = end;
                for(; s != end; t = s++) {
                    vec2f const &p1 = outline_vertices[s];
                    vec2f const &p2 = outline_vertices[t];
                    if(line_intersects_rect(world_rect, p1, p2)) {
                        hit = true;
                        break;
                    }
                }
            }
            if(hit) {
                e.flags |= set_flags;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::flag_enclosed_entities(rect const &world_rect, int clear_flags, int set_flags)
    {
        for(auto &e : entities) {
            e.flags &= ~clear_flags;
            if(world_rect.contains_rect(e.bounds)) {
                e.flags |= set_flags;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::flag_entities_at_point(vec2d point, int clear_flags, int set_flags)
    {
        for(auto &e : entities) {
            e.flags &= ~clear_flags;
            if(e.bounds.contains(point)) {
                auto p = outline_vertices.data() + e.outline_offset;
                int s = e.outline_size;
                if(point_in_poly(p, s, vec2f(point))) {
                    e.flags |= set_flags;
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::clear()
    {
        LOG_DEBUG("BOUNDARY TESSELATOR: Clear");
        entities.clear();         // the entities (each may consist of multiple draw calls (if a macro has disconnected primitives))
        fill_vertices.clear();    // the verts (for outlines and fills)
        fill_indices.clear();          // the indices (for fills)
        fill_spans.clear();            // spans for drawing fills (GL_LINE_LOOP)
        outline_vertices.clear();
        outline_lines.clear();
        entity_flags.clear();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::new_entity(int entity_id, int flags)
    {
        finish_entity();

        entities.emplace_back(entity_id, (int)fill_spans.size(), 0, (int)outline_vertices.size(), 0, flags);
        boundary_stesselator = tessNewTess(nullptr);
        tessSetOption(boundary_stesselator, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);
        tessSetOption(boundary_stesselator, TESS_REVERSE_CONTOURS, 1);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::append_points(size_t offset)
    {
        tesselator_entity &e = entities.back();
        if(e.bounds.is_empty_rect()) {
            e.bounds.min_pos = e.bounds.max_pos = vec2d(temp_points[offset]);
        }
        for(size_t i = offset; i < temp_points.size(); ++i) {
            e.bounds.expand_to_contain(vec2d(temp_points[i]));
        }
        tessAddContour(boundary_stesselator, 2, temp_points.data() + offset, sizeof(float) * 2, (int)(temp_points.size() - offset));
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::finish_entity()
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

            // now, we need start, end vertices so the lines are all independent
            // so we can draw them all with one draw call
            // so, if it's the first point, add to start
            // else add to start and end
            // then, after, add the first point of start to end to make a loop

            for(int i = 0; i < nelems; ++i) {
                // int outline_base = static_cast<int>(outline_vertices_start.size());
                int b = elems[i * 2];
                int n = elems[i * 2 + 1];
                float const *f = &verts[b * 2];
                // float const *s = f;
                tessAddContour(interior_tesselator, 2, f, sizeof(float) * 2, n);
                for(int p = 0; p < n; ++p) {
                    outline_vertices.emplace_back(f[0], f[1]);
                    f += 2;
                }
            }

            tesselator_entity &e = entities.back();
            e.outline_size = (int)(outline_vertices.size() - e.outline_offset);

            tessTesselate(interior_tesselator, TESS_WINDING_POSITIVE, TESS_POLYGONS, 3, 2, nullptr);

            float const *tri_verts = tessGetVertices(interior_tesselator);
            int const tri_nverts = tessGetVertexCount(interior_tesselator);
            int const *tri_elems = tessGetElements(interior_tesselator);
            int const tri_nelems = tessGetElementCount(interior_tesselator);

            size_t base = fill_vertices.size();
            for(int v = 0; v < tri_nverts; ++v) {
                float const *vrt = tri_verts + v * 2;
                fill_vertices.emplace_back(vrt[0], vrt[1]);
            }

            uint32_t index_base = static_cast<uint32_t>(fill_indices.size());

            uint32_t fill_base = static_cast<uint32_t>(fill_spans.size());

            for(int x = 0; x < tri_nelems; ++x) {
                int const *p = &tri_elems[x * 3];
                int const p0 = p[0];
                int const p1 = p[1];
                int const p2 = p[2];
                if(p0 != TESS_UNDEF && p1 != TESS_UNDEF && p2 != TESS_UNDEF) {
                    fill_indices.push_back(static_cast<GLuint>(p0 + base));
                    fill_indices.push_back(static_cast<GLuint>(p1 + base));
                    fill_indices.push_back(static_cast<GLuint>(p2 + base));
                }
            }
            fill_spans.emplace_back(index_base, static_cast<int>(fill_indices.size() - index_base));

            e.num_fills = static_cast<int>(fill_spans.size() - fill_base);

            tessDeleteTess(interior_tesselator);

            tessDeleteTess(boundary_stesselator);
            boundary_stesselator = nullptr;
        }
        temp_points.clear();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::finalize()
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
        clear();
        g->draw(*this);
        new_entity(current_entity_id, current_flag);
        finalize();

        // create the lines index buffer and flags buffer

        entity_flags.resize(entities.size() + 1);
        for(size_t i=0; i<entities.size(); ++i) {
            tesselator_entity &e = entities[i];
            auto id = e.entity_id;
            if(id >= entity_flags.size()) {
                LOG_ERROR("Entity ID out of range!?");
                id = 0;
            }
            entity_flags[id] = 2;
            size_t s = e.outline_offset;
            size_t t = s + e.outline_size - 1;
            size_t u = t;
            for(; s <= u; t = s++) {
                outline_lines.emplace_back((uint32_t)s, (uint32_t)t, (uint32_t)id, 0);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::on_finished_loading()
    {
        if(fill_vertices.empty() || fill_indices.empty() || outline_vertices.empty()) {
            LOG_INFO("Layer {} is empty", this->gerber_file->filename);
            return;
        }

        GL_CHECK(glGenBuffers(3, line_buffers));
        GL_CHECK(glBindBuffer(GL_TEXTURE_BUFFER, line_buffers[0]));
        GL_CHECK(glBufferData(GL_TEXTURE_BUFFER, outline_lines.size() * sizeof(gl_line2_program::line), outline_lines.data(), GL_STATIC_DRAW));
        GL_CHECK(glBindBuffer(GL_TEXTURE_BUFFER, line_buffers[1]));
        GL_CHECK(glBufferData(GL_TEXTURE_BUFFER, outline_vertices.size() * sizeof(vec2f), outline_vertices.data(), GL_STATIC_DRAW));
        GL_CHECK(glBindBuffer(GL_TEXTURE_BUFFER, line_buffers[2]));
        GL_CHECK(glBufferData(GL_TEXTURE_BUFFER, entity_flags.size() * sizeof(uint8_t), entity_flags.data(), GL_DYNAMIC_DRAW));

        glGenTextures(3, textures);
        glBindTexture(GL_TEXTURE_BUFFER, textures[0]);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, line_buffers[0]);
        glBindTexture(GL_TEXTURE_BUFFER, textures[1]);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, line_buffers[1]);
        glBindTexture(GL_TEXTURE_BUFFER, textures[2]);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, line_buffers[2]);

        //////////

        vertex_array.init(static_cast<GLsizei>(fill_vertices.size()));
        vertex_array.activate();
        update_buffer<GL_ARRAY_BUFFER>(fill_vertices);

        index_array.init(static_cast<GLsizei>(fill_indices.size()));
        index_array.activate();
        update_buffer<GL_ELEMENT_ARRAY_BUFFER>(fill_indices);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        using gerber_lib::gerber_draw_element;

        double constexpr THRESHOLD = 1e-38;
        double constexpr ARC_DEGREES = 3.6;

        int flag = polarity == polarity_clear ? entity_flags_t::clear : entity_flags_t::none;

        if(entity_id != current_entity_id) {
            new_entity(entity_id, flag);
        }

        current_flag = flag;
        current_entity_id = entity_id;

        std::vector<vec2f> &points = temp_points;
        size_t offset = points.size();

        auto add_point = [&](double x, double y) {
            if(points.empty() || fabs(points.back().x - x) > THRESHOLD || fabs(points.back().y - y) > THRESHOLD) {
                points.emplace_back(static_cast<float>(x), static_cast<float>(y));
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
        append_points(offset);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill(bool wireframe, bool invert, gl_matrix const &matrix)
    {
        if(vertex_array.num_verts == 0 || index_array.num_indices == 0) {
            return;
        }
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_BLEND);

        gl::color fill_color = gl::colors::red;
        gl::color clear_color = gl::colors::green;

        layer_program->use();

        GL_CHECK(glUniformMatrix4fv(layer_program->u_transform, 1, false, matrix.m));

        vertex_array.activate();
        index_array.activate();

        if(invert) {
            gl::colorf4 f(fill_color);
            std::swap(fill_color, clear_color);
            glClearColor(f.red(), f.green(), f.blue(), f.alpha());
            glClear(GL_COLOR_BUFFER_BIT);
        }

        for(auto const &e : entities) {
            if(wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            if((e.flags & entity_flags_t::clear) != 0) {
                layer_program->set_color(clear_color);
            } else {
                layer_program->set_color(fill_color);
            }
            int end = e.num_fills + e.first_fill;
            for(int i = e.first_fill; i < end; ++i) {
                tesselator_span const &s = fill_spans[i];
                glDrawElements(GL_TRIANGLES, s.length, GL_UNSIGNED_INT, (void *)(s.start * sizeof(GLuint)));
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::outline(float outline_thickness, gl_matrix const &matrix, vec2d const &window_size)
    {
        if(vertex_array.num_verts == 0 || index_array.num_indices == 0) {
            return;
        }
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_BLEND);

        gl::color outline_color = gl::colors::blue;
        gl::color fill_color = gl::colors::red;

        for(auto const &e : entities) {
            int id = e.entity_id;
            if((e.flags & entity_flags_t::hovered) != 0) {
                entity_flags[id] = 1;
            } else {
                entity_flags[id] = 0;
            }
        }
        GL_CHECK(glBindBuffer(GL_TEXTURE_BUFFER, line_buffers[2]));
        update_buffer<GL_TEXTURE_BUFFER>(entity_flags);

        line2_program->use();
        line2_program->quad_points_array.activate();
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
        GL_CHECK(glBindTexture(GL_TEXTURE_BUFFER, textures[0])); // The Lines TBO (RGBA32UI)
        GL_CHECK(glActiveTexture(GL_TEXTURE1));
        GL_CHECK(glBindTexture(GL_TEXTURE_BUFFER, textures[1])); // The Verts TBO (RG32F)
        GL_CHECK(glActiveTexture(GL_TEXTURE2));
        GL_CHECK(glBindTexture(GL_TEXTURE_BUFFER, textures[2])); // The Flags TBO (R8UI)

        GL_CHECK(glUniform1i(line2_program->u_lines_sampler, 0)); // lines_sampler -> GL_TEXTURE0
        GL_CHECK(glUniform1i(line2_program->u_vert_sampler, 1)); // vert_sampler     -> GL_TEXTURE1
        GL_CHECK(glUniform1i(line2_program->u_flags_sampler, 2)); // flags_sampler    -> GL_TEXTURE2
        GL_CHECK(glUniform1f(line2_program->u_thickness, outline_thickness));
        GL_CHECK(glUniform2f(line2_program->u_viewport_size, (float)window_size.x, (float)window_size.y));
        GL_CHECK(glUniformMatrix4fv(line2_program->u_transform, 1, false, matrix.m));
        GL_CHECK(glUniform4fv(line2_program->u_select_color, 1, gl::colorf4(outline_color).f));
        GL_CHECK(glUniform4fv(line2_program->u_hover_color, 1, gl::colorf4(fill_color).f));
        GL_CHECK(glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)outline_lines.size()));
    }

}    // namespace gerber_3d
