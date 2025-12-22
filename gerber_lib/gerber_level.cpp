//////////////////////////////////////////////////////////////////////

#include "gerber_level.h"
#include "gerber_image.h"

LOG_CONTEXT("level", debug);

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    gerber_level::gerber_level(gerber_image *image) : gerber_level()
    {
        if(!image->levels.empty()) {
            gerber_level *previous = image->levels.back();
            name = previous->name;
            step_and_repeat = previous->step_and_repeat;
            polarity = previous->polarity;
            knockout = previous->knockout;    // YOINK!?
            knockout.first_instance = false;
        }
        image->levels.push_back(this);
    }

}    // namespace gerber_lib