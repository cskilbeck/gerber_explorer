//
// X offscreen render
// X ImGui
// X save/restore settings and window state
// X use proper font
// X use matsym icon library
// X use VS2022 to build it
// X ignore initial mouse zoom moves
// X support dropping filenames to load them
// X load layers multithreaded
// X handle empty layers
// X inverted layer
// X drag/drop reorder layers
//
// flip x/y
// picking / selection
// render-on-window-resize
// fix ImGui viewport thing
// gerber spec compliance (polygons, holes)
// load zip file (how to store path in settings?)
// detect gerber layer type/position from extension/name/x2 info
// ?OpenCascade 3D nonsense
// measure tool
// export PNG
// ? undo/redo !?
//

/*
 * file extension (Protel: GTO, GTL etc)
 * filename (from the ok folder from jlcpcb)
 * filename (from KiCad - XXXX_F_Cu.gbr etc)
 * X2 comment (%TF.FileFunction,Soldermask,Top*%)
 *
 * all drills
 * board
 * profile
 * keepout
 * -----
 * top overlay
 * top paste
 * top pads
 * top soldermask (invert)
 * copper 1..n
 * bottom soldermask (invert)
 * bottom pads
 * bottom paste
 * bottom overlay
 * -----
 */

#include <cstdio>
#include <filesystem>

#include "gerber_explorer.h"
#include "settings.h"

LOG_CONTEXT("main", info);

//////////////////////////////////////////////////////////////////////

int flushed_puts(char const *s)
{
    int x = puts(s);
    fflush(stdout);
    return x;
}

int main(int, char **)
{
    log_set_level(gerber_lib::log_level_debug);
    gerber_lib::log_set_emitter_function(flushed_puts);


    gerber_explorer window;
    window.init();

    while(window.update()) {
    }

    return 0;
}
