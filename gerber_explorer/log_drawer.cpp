#include "log_drawer.h"
#include "gerber_lib.h"
#include "gerber_net.h"
#include "gerber_log.h"

LOG_CONTEXT("log_dump", debug);

namespace gl
{
    //////////////////////////////////////////////////////////////////////

    void log_drawer::set_gerber(gerber_lib::gerber_file *g)
    {
        gerber_file = g;
        LOG_INFO("LOGGER IS READY, {} nets in total", g->image.nets.size());
    }

    //////////////////////////////////////////////////////////////////////

    void log_drawer::fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, gerber_lib::gerber_net *gnet)
    {
        LOG_INFO("fill entity:{},elements:{},polarity:{}", gnet->entity_id, num_elements, polarity);
        for(size_t n = 0; n < num_elements; ++n) {
            LOG_INFO("[{}]={}", n, elements[n]);
        }
    }

}    // namespace gerber_3d
