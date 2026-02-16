//////////////////////////////////////////////////////////////////////

#include "gl_3d_drawer.h"

#include "gerber_explorer.h"
#include "gerber_lib.h"
#include "gerber_net.h"
#include "gerber_math.h"

#include "tesselator.h"

#include <cmath>
#include <algorithm>
#include <limits>

LOG_CONTEXT("gl_3d_drawer", info);

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    using namespace gerber_lib;

    //////////////////////////////////////////////////////////////////////

    namespace
    {
        struct vec2f
        {
            float x, y;
        };

        bool is_clockwise(std::vector<vec2f> const &points, size_t start, size_t end)
        {
            double sum = 0;
            for(size_t i = start, n = end - 1; i != end; n = i++) {
                vec2f const &p1 = points[i];
                vec2f const &p2 = points[n];
                sum += (p2.x - p1.x) * (p2.y + p1.y);
            }
            return sum < 0;
        }
    }    // namespace

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::init()
    {
        mesh_vertices.init();
        mesh_indices.init();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::clear()
    {
        pending_contours.clear();
        resolved_tree.Clear();
        mesh_vertices.clear();
        mesh_indices.clear();
        has_mesh = false;
        extruded_depth = 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::release()
    {
        pending_contours.clear();
        pending_contours.shrink_to_fit();
        resolved_tree.Clear();
        mesh_vertices.release();
        mesh_indices.release();
        has_mesh = false;
        extruded_depth = 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::set_gerber(gerber_file *g)
    {
        clear();
        g->draw(*this);
        resolve_2d();
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gl_3d_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, gerber_net *gnet)
    {
        double constexpr THRESHOLD = 1e-38;
        double constexpr ARC_DEGREES_F[tesselation_quality::num_qualities] = { 10, 5, 2 };
        double ARC_DEGREES = ARC_DEGREES_F[tesselation_quality];

        std::vector<vec2f> temp_points;

        auto add_point = [&](double x, double y) {
            if(temp_points.empty() || fabs(temp_points.back().x - x) > THRESHOLD || fabs(temp_points.back().y - y) > THRESHOLD) {
                temp_points.push_back({ static_cast<float>(x), static_cast<float>(y) });
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

        if(temp_points.size() < 3) {
            return ok;
        }

        // remove duplicate closing point
        if(temp_points.back().x == temp_points.front().x && temp_points.back().y == temp_points.front().y) {
            temp_points.pop_back();
        }

        // force counter clockwise ordering
        if(is_clockwise(temp_points, 0, temp_points.size())) {
            std::reverse(temp_points.begin(), temp_points.end());
        }

        // convert to Clipper2 Path64
        Clipper2Lib::Path64 path;
        path.reserve(temp_points.size());
        for(auto const &pt : temp_points) {
            path.push_back(Clipper2Lib::Point64(
                static_cast<int64_t>(pt.x * CLIPPER_SCALE),
                static_cast<int64_t>(pt.y * CLIPPER_SCALE)));
        }

        pending_contours.push_back({ std::move(path), polarity });

        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::resolve_2d()
    {
        using namespace Clipper2Lib;

        if(pending_contours.empty()) {
            return;
        }

        Paths64 result;

        size_t i = 0;
        while(i < pending_contours.size()) {

            auto current_polarity = pending_contours[i].polarity;

            // batch consecutive contours with same polarity
            Paths64 batch;
            while(i < pending_contours.size() && pending_contours[i].polarity == current_polarity) {
                batch.push_back(std::move(pending_contours[i].path));
                i++;
            }

            bool should_fill = current_polarity == polarity_dark || current_polarity == polarity_negative;

            if(result.empty() && should_fill) {
                // first batch of additive material: just union them
                Clipper64 clipper;
                clipper.AddSubject(batch);
                Paths64 temp;
                clipper.Execute(ClipType::Union, FillRule::NonZero, temp);
                result = std::move(temp);
            } else if(!batch.empty()) {
                Clipper64 clipper;
                clipper.AddSubject(result);
                clipper.AddClip(batch);

                ClipType op = (current_polarity == polarity_clear) ? ClipType::Difference : ClipType::Union;

                Paths64 temp;
                clipper.Execute(op, FillRule::NonZero, temp);
                result = std::move(temp);
            }
        }

        // final pass to get PolyTree with outer/hole hierarchy
        {
            Clipper64 clipper;
            clipper.AddSubject(result);
            clipper.Execute(ClipType::Union, FillRule::NonZero, resolved_tree);
        }

        // free intermediate data
        pending_contours.clear();
        pending_contours.shrink_to_fit();

        LOG_INFO("resolve_2d: {} top-level contours", resolved_tree.Count());
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::extrude(double depth)
    {
        extrude(0.0, depth);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::extrude(double z_bot, double z_top)
    {
        mesh_vertices.clear();
        mesh_indices.clear();
        extruded_depth = z_top - z_bot;

        process_polytree_children(resolved_tree, static_cast<float>(z_bot), static_cast<float>(z_top));

        has_mesh = true;
        LOG_INFO("extrude: {} vertices, {} indices ({} triangles)",
                 mesh_vertices.size(), mesh_indices.size(), mesh_indices.size() / 3);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::process_polytree_children(Clipper2Lib::PolyPath64 const &node, float z_bot, float z_top)
    {
        for(auto const &child : node) {
            // each top-level child is an outer boundary
            extrude_polygon(*child, z_bot, z_top);

            // holes may contain nested outers (islands inside holes)
            for(auto const &hole : *child) {
                process_polytree_children(*hole, z_bot, z_top);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::extrude_polygon(Clipper2Lib::PolyPath64 const &outer_node, float z_bot, float z_top)
    {
        double const inv_scale = 1.0 / CLIPPER_SCALE;

        auto path_to_floats = [&](Clipper2Lib::Path64 const &path) -> std::vector<float> {
            std::vector<float> coords;
            coords.reserve(path.size() * 2);
            for(auto const &pt : path) {
                coords.push_back(static_cast<float>(pt.x * inv_scale));
                coords.push_back(static_cast<float>(pt.y * inv_scale));
            }
            return coords;
        };

        // triangulate the polygon (outer + holes) using libtess2
        TESStesselator *tess = tessNewTess(nullptr);
        tessSetOption(tess, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);

        auto outer_coords = path_to_floats(outer_node.Polygon());
        tessAddContour(tess, 2, outer_coords.data(), sizeof(float) * 2, static_cast<int>(outer_node.Polygon().size()));

        for(auto const &hole_child : outer_node) {
            auto hole_coords = path_to_floats(hole_child->Polygon());
            tessAddContour(tess, 2, hole_coords.data(), sizeof(float) * 2, static_cast<int>(hole_child->Polygon().size()));
        }

        if(!tessTesselate(tess, TESS_WINDING_ODD, TESS_POLYGONS, 3, 2, nullptr)) {
            tessDeleteTess(tess);
            return;
        }

        float const *verts = tessGetVertices(tess);
        int const nverts = tessGetVertexCount(tess);
        int const *elems = tessGetElements(tess);
        int const nelems = tessGetElementCount(tess);

        // bottom cap (z = z_bot)
        uint32_t base_bot = static_cast<uint32_t>(mesh_vertices.size());
        for(int v = 0; v < nverts; v++) {
            mesh_vertices.push_back({ verts[v * 2], verts[v * 2 + 1], z_bot });
        }
        for(int t = 0; t < nelems; t++) {
            int const *tri = &elems[t * 3];
            if(tri[0] != TESS_UNDEF && tri[1] != TESS_UNDEF && tri[2] != TESS_UNDEF) {
                // bottom face: normal points -Z, so reverse winding
                mesh_indices.push_back(base_bot + tri[0]);
                mesh_indices.push_back(base_bot + tri[2]);
                mesh_indices.push_back(base_bot + tri[1]);
            }
        }

        // top cap (z = z_top)
        uint32_t base_top = static_cast<uint32_t>(mesh_vertices.size());
        for(int v = 0; v < nverts; v++) {
            mesh_vertices.push_back({ verts[v * 2], verts[v * 2 + 1], z_top });
        }
        for(int t = 0; t < nelems; t++) {
            int const *tri = &elems[t * 3];
            if(tri[0] != TESS_UNDEF && tri[1] != TESS_UNDEF && tri[2] != TESS_UNDEF) {
                // top face: normal points +Z, standard winding
                mesh_indices.push_back(base_top + tri[0]);
                mesh_indices.push_back(base_top + tri[1]);
                mesh_indices.push_back(base_top + tri[2]);
            }
        }

        tessDeleteTess(tess);

        // side walls
        auto add_side_walls = [&](Clipper2Lib::Path64 const &contour, bool is_hole) {
            size_t n = contour.size();
            for(size_t i = 0; i < n; i++) {
                size_t j = (i + 1) % n;
                float x0 = static_cast<float>(contour[i].x * inv_scale);
                float y0 = static_cast<float>(contour[i].y * inv_scale);
                float x1 = static_cast<float>(contour[j].x * inv_scale);
                float y1 = static_cast<float>(contour[j].y * inv_scale);

                uint32_t base = static_cast<uint32_t>(mesh_vertices.size());
                mesh_vertices.push_back({ x0, y0, z_bot });    // 0
                mesh_vertices.push_back({ x1, y1, z_bot });    // 1
                mesh_vertices.push_back({ x1, y1, z_top });    // 2
                mesh_vertices.push_back({ x0, y0, z_top });    // 3

                if(is_hole) {
                    // hole contours wind clockwise, so walls face inward
                    mesh_indices.push_back(base + 0);
                    mesh_indices.push_back(base + 2);
                    mesh_indices.push_back(base + 1);
                    mesh_indices.push_back(base + 0);
                    mesh_indices.push_back(base + 3);
                    mesh_indices.push_back(base + 2);
                } else {
                    // outer contours wind counter-clockwise, walls face outward
                    mesh_indices.push_back(base + 0);
                    mesh_indices.push_back(base + 1);
                    mesh_indices.push_back(base + 2);
                    mesh_indices.push_back(base + 0);
                    mesh_indices.push_back(base + 2);
                    mesh_indices.push_back(base + 3);
                }
            }
        };

        add_side_walls(outer_node.Polygon(), false);
        for(auto const &hole_child : outer_node) {
            add_side_walls(hole_child->Polygon(), true);
        }
    }

    //////////////////////////////////////////////////////////////////////

    namespace
    {
        double point_to_segment_distance_sq(int64_t px, int64_t py, int64_t ax, int64_t ay, int64_t bx, int64_t by)
        {
            double dx = static_cast<double>(bx - ax);
            double dy = static_cast<double>(by - ay);
            double len_sq = dx * dx + dy * dy;

            double t = 0;
            if(len_sq > 0) {
                t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
                if(t < 0) t = 0;
                else if(t > 1) t = 1;
            }

            double cx = ax + t * dx - px;
            double cy = ay + t * dy - py;
            return cx * cx + cy * cy;
        }

        // spatial grid for fast nearest-edge distance queries
        struct edge_grid
        {
            struct segment
            {
                int64_t ax, ay, bx, by;
            };

            int64_t origin_x{}, origin_y{};
            double cell_size{};
            double query_radius{};    // max distance we need to find
            int search_r{};           // cell radius to search
            int nx{}, ny{};
            std::vector<std::vector<segment>> cells;

            static constexpr int MAX_GRID_DIM = 512;

            void build(Clipper2Lib::Paths64 const &paths, double max_query_dist)
            {
                query_radius = max_query_dist;
                if(query_radius <= 0) return;

                int64_t lo_x = INT64_MAX, lo_y = INT64_MAX;
                int64_t hi_x = INT64_MIN, hi_y = INT64_MIN;
                for(auto const &path : paths) {
                    for(auto const &pt : path) {
                        if(pt.x < lo_x) lo_x = pt.x;
                        if(pt.y < lo_y) lo_y = pt.y;
                        if(pt.x > hi_x) hi_x = pt.x;
                        if(pt.y > hi_y) hi_y = pt.y;
                    }
                }

                double span_x = static_cast<double>(hi_x - lo_x);
                double span_y = static_cast<double>(hi_y - lo_y);
                double max_span = std::max(span_x, span_y);
                if(max_span <= 0) return;

                // cell size: at least query_radius, scaled so grid doesn't exceed MAX_GRID_DIM
                cell_size = std::max(query_radius, max_span / MAX_GRID_DIM);
                search_r = static_cast<int>(std::ceil(query_radius / cell_size)) + 1;

                origin_x = lo_x;
                origin_y = lo_y;
                nx = std::min(MAX_GRID_DIM, static_cast<int>(span_x / cell_size) + 3);
                ny = std::min(MAX_GRID_DIM, static_cast<int>(span_y / cell_size) + 3);
                cells.resize(static_cast<size_t>(nx) * ny);

                for(auto const &path : paths) {
                    size_t n = path.size();
                    for(size_t i = 0; i < n; i++) {
                        size_t j = (i + 1) % n;
                        segment seg{ path[i].x, path[i].y, path[j].x, path[j].y };

                        int gx0 = static_cast<int>((std::min(seg.ax, seg.bx) - origin_x) / cell_size);
                        int gy0 = static_cast<int>((std::min(seg.ay, seg.by) - origin_y) / cell_size);
                        int gx1 = static_cast<int>((std::max(seg.ax, seg.bx) - origin_x) / cell_size);
                        int gy1 = static_cast<int>((std::max(seg.ay, seg.by) - origin_y) / cell_size);

                        gx0 = std::max(0, gx0);
                        gy0 = std::max(0, gy0);
                        gx1 = std::min(nx - 1, gx1);
                        gy1 = std::min(ny - 1, gy1);

                        for(int gy = gy0; gy <= gy1; gy++) {
                            for(int gx = gx0; gx <= gx1; gx++) {
                                cells[static_cast<size_t>(gy) * nx + gx].push_back(seg);
                            }
                        }
                    }
                }
            }

            double min_distance(int64_t px, int64_t py) const
            {
                int gx = static_cast<int>((px - origin_x) / cell_size);
                int gy = static_cast<int>((py - origin_y) / cell_size);

                double best_sq = query_radius * query_radius * 4;

                int r = search_r;
                for(int dy = -r; dy <= r; dy++) {
                    for(int dx = -r; dx <= r; dx++) {
                        int cx = gx + dx, cy = gy + dy;
                        if(cx < 0 || cx >= nx || cy < 0 || cy >= ny) continue;
                        for(auto const &seg : cells[static_cast<size_t>(cy) * nx + cx]) {
                            double d = point_to_segment_distance_sq(px, py, seg.ax, seg.ay, seg.bx, seg.by);
                            if(d < best_sq) best_sq = d;
                        }
                    }
                }
                return std::sqrt(best_sq);
            }
        };
    }    // namespace

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::extrude_flat_region(Clipper2Lib::Paths64 const &paths, float z_bot, float z_top)
    {
        using namespace Clipper2Lib;

        if(paths.empty()) {
            return;
        }

        // build a PolyTree from the paths so we get outer/hole hierarchy
        PolyTree64 tree;
        {
            Clipper64 clipper;
            clipper.AddSubject(paths);
            clipper.Execute(ClipType::Union, FillRule::NonZero, tree);
        }

        process_polytree_children(tree, z_bot, z_top);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::extrude_conformal_region(Clipper2Lib::Paths64 const &band_paths,
                                                Clipper2Lib::Paths64 const &reference_paths,
                                                double draft_width,
                                                float z_bot_on, float z_top_on,
                                                float z_bot_off, float z_top_off)
    {
        using namespace Clipper2Lib;

        if(band_paths.empty()) {
            return;
        }

        double const inv_scale = 1.0 / CLIPPER_SCALE;
        double const dw_scaled = draft_width * CLIPPER_SCALE;

        // build spatial grid for fast distance queries
        edge_grid grid;
        grid.build(reference_paths, dw_scaled);

        // all vertices in band_paths are outside the reference, so t = distance / draft_width
        auto compute_t = [&](int64_t px, int64_t py) -> float {
            double dist = grid.min_distance(px, py);
            double t = (dw_scaled > 0) ? dist / dw_scaled : 1.0;
            if(t > 1.0) t = 1.0;
            return static_cast<float>(t);
        };

        auto lerp = [](float a, float b, float t) -> float { return a + t * (b - a); };

        PolyTree64 tree;
        {
            Clipper64 clipper;
            clipper.AddSubject(band_paths);
            clipper.Execute(ClipType::Union, FillRule::NonZero, tree);
        }

        std::function<void(PolyPath64 const &)> process_node;
        process_node = [&](PolyPath64 const &node) {
            for(auto const &outer_child : node) {

                auto path_to_floats = [&](Path64 const &path) -> std::vector<float> {
                    std::vector<float> coords;
                    coords.reserve(path.size() * 2);
                    for(auto const &pt : path) {
                        coords.push_back(static_cast<float>(pt.x * inv_scale));
                        coords.push_back(static_cast<float>(pt.y * inv_scale));
                    }
                    return coords;
                };

                TESStesselator *tess = tessNewTess(nullptr);
                tessSetOption(tess, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);

                auto outer_coords = path_to_floats(outer_child->Polygon());
                tessAddContour(tess, 2, outer_coords.data(), sizeof(float) * 2,
                               static_cast<int>(outer_child->Polygon().size()));

                for(auto const &hole_child : *outer_child) {
                    auto hole_coords = path_to_floats(hole_child->Polygon());
                    tessAddContour(tess, 2, hole_coords.data(), sizeof(float) * 2,
                                   static_cast<int>(hole_child->Polygon().size()));
                }

                if(!tessTesselate(tess, TESS_WINDING_ODD, TESS_POLYGONS, 3, 2, nullptr)) {
                    tessDeleteTess(tess);
                    goto next;
                }

                {
                    float const *verts = tessGetVertices(tess);
                    int const nverts = tessGetVertexCount(tess);
                    int const *elems = tessGetElements(tess);
                    int const nelems = tessGetElementCount(tess);

                    std::vector<float> vt(nverts);
                    for(int v = 0; v < nverts; v++) {
                        int64_t cx = static_cast<int64_t>(verts[v * 2] * CLIPPER_SCALE);
                        int64_t cy = static_cast<int64_t>(verts[v * 2 + 1] * CLIPPER_SCALE);
                        vt[v] = compute_t(cx, cy);
                    }

                    // bottom cap
                    uint32_t base_bot = static_cast<uint32_t>(mesh_vertices.size());
                    for(int v = 0; v < nverts; v++) {
                        mesh_vertices.push_back({ verts[v * 2], verts[v * 2 + 1], lerp(z_bot_on, z_bot_off, vt[v]) });
                    }
                    for(int t = 0; t < nelems; t++) {
                        int const *tri = &elems[t * 3];
                        if(tri[0] != TESS_UNDEF && tri[1] != TESS_UNDEF && tri[2] != TESS_UNDEF) {
                            mesh_indices.push_back(base_bot + tri[0]);
                            mesh_indices.push_back(base_bot + tri[2]);
                            mesh_indices.push_back(base_bot + tri[1]);
                        }
                    }

                    // top cap
                    uint32_t base_top = static_cast<uint32_t>(mesh_vertices.size());
                    for(int v = 0; v < nverts; v++) {
                        mesh_vertices.push_back({ verts[v * 2], verts[v * 2 + 1], lerp(z_top_on, z_top_off, vt[v]) });
                    }
                    for(int t = 0; t < nelems; t++) {
                        int const *tri = &elems[t * 3];
                        if(tri[0] != TESS_UNDEF && tri[1] != TESS_UNDEF && tri[2] != TESS_UNDEF) {
                            mesh_indices.push_back(base_top + tri[0]);
                            mesh_indices.push_back(base_top + tri[1]);
                            mesh_indices.push_back(base_top + tri[2]);
                        }
                    }

                    tessDeleteTess(tess);

                    // side walls with per-vertex Z
                    auto add_side_walls = [&](Path64 const &contour, bool is_hole) {
                        size_t n = contour.size();
                        for(size_t i = 0; i < n; i++) {
                            size_t j = (i + 1) % n;
                            float x0 = static_cast<float>(contour[i].x * inv_scale);
                            float y0 = static_cast<float>(contour[i].y * inv_scale);
                            float x1 = static_cast<float>(contour[j].x * inv_scale);
                            float y1 = static_cast<float>(contour[j].y * inv_scale);

                            float t0 = compute_t(contour[i].x, contour[i].y);
                            float t1 = compute_t(contour[j].x, contour[j].y);

                            uint32_t base = static_cast<uint32_t>(mesh_vertices.size());
                            mesh_vertices.push_back({ x0, y0, lerp(z_bot_on, z_bot_off, t0) });
                            mesh_vertices.push_back({ x1, y1, lerp(z_bot_on, z_bot_off, t1) });
                            mesh_vertices.push_back({ x1, y1, lerp(z_top_on, z_top_off, t1) });
                            mesh_vertices.push_back({ x0, y0, lerp(z_top_on, z_top_off, t0) });

                            if(is_hole) {
                                mesh_indices.push_back(base + 0);
                                mesh_indices.push_back(base + 2);
                                mesh_indices.push_back(base + 1);
                                mesh_indices.push_back(base + 0);
                                mesh_indices.push_back(base + 3);
                                mesh_indices.push_back(base + 2);
                            } else {
                                mesh_indices.push_back(base + 0);
                                mesh_indices.push_back(base + 1);
                                mesh_indices.push_back(base + 2);
                                mesh_indices.push_back(base + 0);
                                mesh_indices.push_back(base + 2);
                                mesh_indices.push_back(base + 3);
                            }
                        }
                    };

                    add_side_walls(outer_child->Polygon(), false);
                    for(auto const &hole_child : *outer_child) {
                        add_side_walls(hole_child->Polygon(), true);
                    }
                }

            next:
                for(auto const &hole_child : *outer_child) {
                    process_node(*hole_child);
                }
            }
        };

        process_node(tree);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::extrude_two_shell(Clipper2Lib::Paths64 const &reference_paths,
                                         double z_bot_on_ref, double z_top_on_ref,
                                         double z_bot_off_ref, double z_top_off_ref)
    {
        using namespace Clipper2Lib;

        mesh_vertices.clear();
        mesh_indices.clear();

        Paths64 layer_paths = PolyTreeToPaths64(resolved_tree);
        if(layer_paths.empty()) {
            has_mesh = true;
            return;
        }

        // draft width for ~45 degree slope = max Z step between on/off reference
        double draft_width = std::max(std::abs(z_top_on_ref - z_top_off_ref),
                                      std::abs(z_bot_on_ref - z_bot_off_ref));
        double draft_scaled = draft_width * CLIPPER_SCALE;

        Paths64 inflated_ref = InflatePaths(reference_paths, draft_scaled,
                                            JoinType::Round, EndType::Polygon);

        LOG_INFO("extrude_two_shell: {} layer paths, {} reference paths, draft_width={:.4f}",
                 layer_paths.size(), reference_paths.size(), draft_width);

        float const z_bot_on = static_cast<float>(z_bot_on_ref);
        float const z_top_on = static_cast<float>(z_top_on_ref);
        float const z_bot_off = static_cast<float>(z_bot_off_ref);
        float const z_top_off = static_cast<float>(z_top_off_ref);

        // region 1: layer over copper → flat at on_ref Z
        // output directly to PolyTree to avoid redundant Union in extrude_flat_region
        {
            PolyTree64 on_ref_tree;
            Clipper64 clipper;
            clipper.AddSubject(layer_paths);
            clipper.AddClip(reference_paths);
            clipper.Execute(ClipType::Intersection, FillRule::NonZero, on_ref_tree);
            LOG_INFO("extrude_two_shell: on_ref tree {} outers", on_ref_tree.Count());
            process_polytree_children(on_ref_tree, z_bot_on, z_top_on);
        }
        LOG_INFO("extrude_two_shell: after on_ref: {} verts, {} tris",
                 mesh_vertices.size(), mesh_indices.size() / 3);

        // region 2: layer beyond draft zone → flat at off_ref Z
        {
            PolyTree64 off_ref_tree;
            Clipper64 clipper;
            clipper.AddSubject(layer_paths);
            clipper.AddClip(inflated_ref);
            clipper.Execute(ClipType::Difference, FillRule::NonZero, off_ref_tree);
            LOG_INFO("extrude_two_shell: off_ref tree {} outers", off_ref_tree.Count());
            process_polytree_children(off_ref_tree, z_bot_off, z_top_off);
        }
        LOG_INFO("extrude_two_shell: after off_ref: {} verts, {} tris",
                 mesh_vertices.size(), mesh_indices.size() / 3);

        // region 3: draft band = (layer ∩ inflated_ref) - reference → sloped transition
        Paths64 in_inflated;
        {
            Clipper64 clipper;
            clipper.AddSubject(layer_paths);
            clipper.AddClip(inflated_ref);
            clipper.Execute(ClipType::Intersection, FillRule::NonZero, in_inflated);
        }
        Paths64 draft_band;
        {
            Clipper64 clipper;
            clipper.AddSubject(in_inflated);
            clipper.AddClip(reference_paths);
            clipper.Execute(ClipType::Difference, FillRule::NonZero, draft_band);
        }
        LOG_INFO("extrude_two_shell: draft_band {} paths", draft_band.size());

        extrude_conformal_region(draft_band, reference_paths, draft_width,
                                 z_bot_on, z_top_on, z_bot_off, z_top_off);

        has_mesh = true;
        LOG_INFO("extrude_two_shell: {} vertices, {} indices ({} triangles)",
                 mesh_vertices.size(), mesh_indices.size(), mesh_indices.size() / 3);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_3d_drawer::export_stl(std::string const &filename) const
    {
        if(!has_mesh) {
            LOG_ERROR("export_stl: no mesh data");
            return;
        }

        FILE *f = fopen(filename.c_str(), "wb");
        if(!f) {
            LOG_ERROR("export_stl: failed to open {}", filename);
            return;
        }

        char header[80] = {};
        snprintf(header, sizeof(header), "gerber_explorer STL export");
        fwrite(header, 80, 1, f);

        uint32_t num_triangles = static_cast<uint32_t>(mesh_indices.size() / 3);
        fwrite(&num_triangles, 4, 1, f);

        for(uint32_t t = 0; t < num_triangles; t++) {
            vec3f const &v0 = mesh_vertices[mesh_indices[t * 3 + 0]];
            vec3f const &v1 = mesh_vertices[mesh_indices[t * 3 + 1]];
            vec3f const &v2 = mesh_vertices[mesh_indices[t * 3 + 2]];

            // face normal via cross product
            float ax = v1.x - v0.x, ay = v1.y - v0.y, az = v1.z - v0.z;
            float bx = v2.x - v0.x, by = v2.y - v0.y, bz = v2.z - v0.z;
            float nx = ay * bz - az * by;
            float ny = az * bx - ax * bz;
            float nz = ax * by - ay * bx;
            float len = sqrtf(nx * nx + ny * ny + nz * nz);
            if(len > 0) {
                nx /= len;
                ny /= len;
                nz /= len;
            }

            fwrite(&nx, 4, 1, f);
            fwrite(&ny, 4, 1, f);
            fwrite(&nz, 4, 1, f);
            fwrite(&v0, 12, 1, f);
            fwrite(&v1, 12, 1, f);
            fwrite(&v2, 12, 1, f);
            uint16_t attr = 0;
            fwrite(&attr, 2, 1, f);
        }

        fclose(f);
        LOG_INFO("export_stl: wrote {} triangles to {}", num_triangles, filename);
    }

}    // namespace gerber_3d
