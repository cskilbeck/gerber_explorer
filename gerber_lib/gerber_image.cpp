//////////////////////////////////////////////////////////////////////

#include "gerber_aperture.h"
#include "gerber_net.h"
#include "gerber_level.h"
#include "gerber_image.h"

#include "gerber_lib.h"

LOG_CONTEXT("gerber_image", info);

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    void gerber_image::cleanup()
    {
        LOG_INFO("cleanup {}", this->info.image_name);

        stats.cleanup();

        for(auto p : aperture_macros) {
            delete p;
        }
        aperture_macros.clear();

        for(auto p : apertures) {
            delete p.second;
        }
        apertures.clear();

        for(auto n : nets) {
            delete n;
        }
        nets.clear();

        for(auto l : levels) {
            delete l;
        }
        levels.clear();

        for(auto ns : net_states) {
            delete ns;
        }
        net_states.clear();
    }

    //////////////////////////////////////////////////////////////////////

    gerber_image ::~gerber_image()
    {
        cleanup();
    }

}    // namespace gerber_lib
