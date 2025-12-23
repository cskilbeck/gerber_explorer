//////////////////////////////////////////////////////////////////////
// LOG renderer for debugging the gerber nets

#pragma once

#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_draw.h"

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct log_drawer final : gerber_lib::gerber_draw_interface
    {
        log_drawer() = default;

        void set_gerber(gerber_lib::gerber *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        void finish_element(int entity_id) override
        {
        }


        void on_finished_loading() override
        {
        }

        gerber_lib::gerber *gerber_file{ nullptr };
    };

}    // namespace gerber_3d
