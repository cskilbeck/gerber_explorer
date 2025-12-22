//////////////////////////////////////////////////////////////////////

#include "gerber_enums.h"
#include "gerber_aperture.h"
#include "gerber_net.h"
#include "gerber_level.h"
#include "gerber_image.h"

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    void gerber_image::cleanup()
    {
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
