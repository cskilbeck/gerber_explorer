//////////////////////////////////////////////////////////////////////
// 3D geometry generator for gerber layers
// Converts 2D gerber entities into an extruded 3D mesh

#pragma once

#include "gerber_lib.h"
#include "gerber_draw.h"
#include "gerber_net.h"
#include "gerber_arena.h"

#include "clipper2/clipper.h"

#include "gerber_log.h"

struct gerber_layer;

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    template <typename T> using typed_arena = gerber_lib::typed_arena<T>;

    using tesselation_quality_t = unsigned int;

    namespace tesselation_quality
    {
        tesselation_quality_t constexpr low = 0;
        tesselation_quality_t constexpr medium = 1;
        tesselation_quality_t constexpr high = 2;
        tesselation_quality_t constexpr num_qualities = 3;
    }    // namespace tesselation_quality

    //////////////////////////////////////////////////////////////////////

    struct gl_3d_drawer : gerber_lib::gerber_draw_interface
    {
        struct vec3f
        {
            float x, y, z;
        };

        struct contour_entry
        {
            Clipper2Lib::Path64 path;
            gerber_lib::gerber_polarity polarity;
        };

        gl_3d_drawer() = default;

        void init();

        // gerber_draw_interface
        void set_gerber(gerber_lib::gerber_file *g) override;
        [[nodiscard]] gerber_lib::gerber_error_code fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements,
                                                                   gerber_lib::gerber_polarity polarity, gerber_lib::gerber_net *gnet) override;

        // process accumulated 2D contours into final result via Clipper2
        void resolve_2d();

        // extrude the 2D result into a 3D mesh
        void extrude(double depth);
        void extrude(double z_bot, double z_top);

        // two-shell extrusion: one shell over reference (e.g. copper), one off reference (e.g. bare substrate)
        void extrude_two_shell(Clipper2Lib::Paths64 const &reference_paths,
                               double z_bot_on_ref, double z_top_on_ref,
                               double z_bot_off_ref, double z_top_off_ref);

        // export
        void export_stl(std::string const &filename) const;

        void clear();
        void release();

        // settings
        tesselation_quality_t tesselation_quality{ tesselation_quality::medium };

        static constexpr int64_t CLIPPER_SCALE = 1000000;

        // intermediate 2D data (pending contours from fill_elements)
        std::vector<contour_entry> pending_contours;

        // final 2D result after Clipper2 booleans
        Clipper2Lib::PolyTree64 resolved_tree;

        // 3D mesh output
        typed_arena<vec3f> mesh_vertices;
        typed_arena<uint32_t> mesh_indices;
        double extruded_depth{};
        bool has_mesh{};

    private:
        void extrude_polygon(Clipper2Lib::PolyPath64 const &outer_node, float z_bot, float z_top);
        void process_polytree_children(Clipper2Lib::PolyPath64 const &node, float z_bot, float z_top);

        void extrude_flat_region(Clipper2Lib::Paths64 const &paths, float z_bot, float z_top);

        void extrude_conformal_region(Clipper2Lib::Paths64 const &region_paths,
                                      Clipper2Lib::Paths64 const &reference_paths,
                                      double draft_width,
                                      float z_bot_on, float z_top_on,
                                      float z_bot_off, float z_top_off);
    };

}    // namespace gerber_3d
