//
// X offscreen render
// X ImGui
// X save/restore settings and window state
// X use proper font
// X use matsym icon library
// X use VS2022 to build it
//
// load layers multithreaded but maintain ordering
// fix gl 1282 for empty layer (E:\dev\clock_monsieur\pcb\Outputs\2025-12-19\clock_led_Soldermask_Top.gbr)
// inverted layer
// picking / selection
// render-on-window-resize
// fix ImGui viewport thing
// gerber spec compliance (polygons, holes)
// load zip file (how to store path in settings?)
// detect gerber layer type/position from extension/name/x2 info
// ?OpenCascade 3D nonsense
// measure tool
// export PNG
//

#include <cstdio>
#include <filesystem>

#include "gerber_lib.h"
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
