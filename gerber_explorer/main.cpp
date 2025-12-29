//
// X offscreen render
//
// ImGui
// inverted layer
// proper picking / selection
// render-on-window-resize
// gerber spec compliance (polygons, holes)
// save/restore settings
// load zip file
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

std::filesystem::path config_path(const std::string &filename);

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
