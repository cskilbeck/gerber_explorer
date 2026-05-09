//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include <cstdint>

#include "gerber_lib.h"
#include "gerber_draw.h"
#include "gerber_net.h"
#include "gerber_arena.h"
#include "gpu_matrix.h"
#include "gpu_base.h"

#include "clipper2/clipper.h"

#include "tesselator.h"

#include "gerber_log.h"

struct gerber_layer;

namespace gerber
{
    //////////////////////////////////////////////////////////////////////

    template <typename T> using typed_arena = gerber_lib::typed_arena<T>;

    namespace entity_flags_t
    {
        uint8_t constexpr none = 0;
        uint8_t constexpr clear = (1 << 0);       // remove material
        uint8_t constexpr fill = (1 << 1);        // add material
        uint8_t constexpr hovered = (1 << 2);     // mouse hovering over it
        uint8_t constexpr selected = (1 << 3);    // it's selected
        uint8_t constexpr active = (1 << 4);      // there can be only one active entity (click cycles through entities under the mouse position)

        uint8_t constexpr all_select = hovered | selected | active;
    }    // namespace entity_flags_t

    //////////////////////////////////////////////////////////////////////

    using tesselation_quality_t = unsigned int;

    namespace tesselation_quality
    {
        tesselation_quality_t constexpr low = 0;
        tesselation_quality_t constexpr medium = 1;
        tesselation_quality_t constexpr high = 2;
        tesselation_quality_t constexpr num_qualities = 3;
    }    // namespace tesselation_quality

    //////////////////////////////////////////////////////////////////////

    static char const *tesselation_quality_name(tesselation_quality_t q)
    {
        static constexpr char const *names[tesselation_quality::num_qualities] = { "Low", "Medium", "High" };
        if(q < std::size(names)) {
            return names[q];
        }
        return "?Unknown";
    }

    //////////////////////////////////////////////////////////////////////

    struct tess_arena_t : gerber_lib::gerber_arena<1ULL << 30, 16>
    {
        tess_arena_t() : gerber_arena()
        {
            memset(&tess_alloc, 0, sizeof(tess_alloc));
            tess_alloc.memalloc = tess_allocate;
            tess_alloc.memfree = tess_free;
            tess_alloc.userData = (void *)this;
        }

        static void *tess_allocate(void *userData, unsigned int size)
        {
            auto arena = (tess_arena_t *)userData;
            return arena->alloc(size);
        }

        static void tess_free([[maybe_unused]] void *userData, [[maybe_unused]] void *ptr)
        {
        }

        TESSalloc tess_alloc{};
    };

    //////////////////////////////////////////////////////////////////////

    struct tesselator_entity
    {
        gerber_lib::gerber_net *net{};
        int outline_offset;                  // offset into outline vertices
        int outline_size{};                  // # of vertices in the outline
        int contour_offset{};               // offset into contour_sizes arena
        int num_contours{};                 // # of contours
        int flags;                           // see entity_flags_t
        gerber_lib::rect bounds{};           // for picking speedup

        int entity_id() const
        {
            return net->entity_id;
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct solid_shape
    {
        typed_arena<gpu::vertex_solid> vertices;
        typed_arena<uint32_t> indices;

        void init()
        {
            vertices.init();
            indices.init();
        }

        void release()
        {
            vertices.release();
            indices.release();
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct gerber_drawer : gerber_lib::gerber_draw_interface
    {
        using vec2f = gerber_lib::vec2f;
        using vert = vec2f;

        gerber_drawer() = default;

        ~gerber_drawer() = default;

        void init(gerber_layer const *for_layer)
        {
            layer = for_layer;
            boundary_arena.init();
            interior_arena.init();
            entities.init();
            temp_points.init();
            outline_lines.init();
            outline_vertices.init();
            entity_flags.init();
            fill_vertices.init();
            fill_indices.init();
            contour_sizes.init();
        }

        // setup from a parsed gerber file
        void set_gerber(gerber_lib::gerber_file *g) override;

        // callback to create draw calls from elements
        gerber_lib::gerber_error_code fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity,
                                                    gerber_lib::gerber_net *gnet) override;

        // admin for tesselation etc
        void clear();
        void new_entity(gerber_lib::gerber_net *net, int flags);
        void append_points(size_t offset);
        void finish_entity();
        void finalize();

        // picking/selection
        void clear_entity_flags(int flags);
        int flag_entities_at_point(gerber_lib::vec2d point, int clear_flags, int set_flags);
        int flag_touching_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        int flag_enclosed_entities(gerber_lib::rect const &world_rect, int clear_flags, int set_flags);
        void find_entities_at_point(gerber_lib::vec2d point, std::vector<int> &indices);
        void select_hovered_entities();

        void release();
        void create_mask();

        gerber_layer const *layer{};
        bool got_mask{ false };
        std::string const &name() const;
        solid_shape mask{};    // only used if it's an outline layer

        // ===== TESSELATION =====
        tesselation_quality_t tesselation_quality;
        double pixels_per_world_unit{0};  // 0 = use fixed quality table, >0 = dynamic (0.5px error)
        int current_flag{ entity_flags_t::none };
        int base_vert{};
        int current_entity_id{ -1 };
        TESStesselator *boundary_stesselator{};
        tess_arena_t boundary_arena;
        tess_arena_t interior_arena;
        typed_arena<tesselator_entity> entities;
        typed_arena<int> contour_sizes;
        typed_arena<vec2f> temp_points;
        typed_arena<gpu::line_instance> outline_lines;
        typed_arena<vec2f> outline_vertices;
        typed_arena<uint8_t> entity_flags;    // one byte per entity
        typed_arena<gpu::vertex_entity> fill_vertices;
        typed_arena<uint32_t> fill_indices;
    };

}    // namespace gerber
