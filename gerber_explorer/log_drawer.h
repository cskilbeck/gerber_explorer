//////////////////////////////////////////////////////////////////////
// LOG renderer for debugging the gerber nets

#pragma once

#include "gerber_lib.h"
#include "gerber_draw.h"

namespace gl
{
    //////////////////////////////////////////////////////////////////////

    struct log_drawer final : gerber_lib::gerber_draw_interface
    {
        log_drawer() = default;

        void set_gerber(gerber_lib::gerber_file *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, gerber_lib::gerber_net *gnet) override;

        gerber_lib::gerber_file *gerber_file{ nullptr };
    };

}    // namespace gerber_3d
