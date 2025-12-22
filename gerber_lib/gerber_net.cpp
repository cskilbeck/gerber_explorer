//////////////////////////////////////////////////////////////////////

#include "gerber_net.h"
#include "gerber_lib.h"
#include "gerber_level.h"
#include "gerber_image.h"
#include "gerber_aperture.h"

LOG_CONTEXT("net", debug);

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    std::string gerber_net_state::to_string() const
    {
        return std::format("NET_STATE: AXIS_SELECT: {}, MIRROR: {}, UNIT: {}, OFFSET: {}, SCALE: {}", axis_select, mirror_state, unit, offset, scale);
    }

    //////////////////////////////////////////////////////////////////////

    std::string gerber_net::to_string() const
    {
        std::string s;

        switch(interpolation_method) {

        case interpolation_linear:
            s = std::format("from {} to {}", start, end);
            break;

        case interpolation_clockwise_circular:
        case interpolation_counterclockwise_circular:
            s = std::format("{}", circle_segment);
            break;

        case interpolation_region_start:
            break;

        case interpolation_region_end:
            break;
        }
        return std::format("NET: APERTURE_STATE: {}, {} {}", aperture_state, interpolation_method, s);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_net_state::gerber_net_state(gerber_image *img) : gerber_net_state()
    {
        img->net_states.push_back(this);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_net::gerber_net(gerber_image *img) : gerber_net()
    {
        level = new gerber_level(img);
        net_state = new gerber_net_state(img);
        img->nets.push_back(this);
    }

    //////////////////////////////////////////////////////////////////////

    gerber_net::gerber_net(gerber_image *img, gerber_net *cur_net, gerber_level *lvl, gerber_net_state *state) : gerber_net()
    {
        if(lvl != nullptr) {
            level = lvl;
        } else {
            level = cur_net->level;
        }

        if(state != nullptr) {
            net_state = state;
        } else {
            net_state = cur_net->net_state;
        }
        img->nets.push_back(this);
    }

}    // namespace gerber_lib