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
// X flip x/y
// X zoom to fit on load
// X load/save settings
// X hotkeys
// X win32 outputs logging when run in a console
// X thick outline
// X fix fit_to_window on load
// X put shaders in separate source files
//
// picking / selection
// separate gl_vertex_array / gl_vertex_buffer
// reduce gpu usage - only draw frame when necessary
// high DPI
// grid
// render-on-window-resize
// fix ImGui viewport thing
// gerber spec compliance (polygons, holes)
// load zip file (how to store path in settings?)
// detect gerber layer type/position from extension/name/x2 info
// measure tool
// export PNG
// ? OpenCascade 3D nonsense ?
// ? undo/redo ?
//

/*
 * file extension (Protel: GTO, GTL etc)
 * filename (from KiCad - XXXX_F_Cu.gbr etc)
 * X2 comment (%TF.FileFunction,Soldermask,Top*%)
 * filename (from the ok folder from jlcpcb)
 *
 * drills
 * board
 * profile
 * keepout
 * -----
 * top overlay
 * top soldermask (invert)
 * top paste
 * top pads
 *
 * copper 1..n
 *
 * bottom pads
 * bottom paste
 * bottom soldermask (invert)
 * bottom overlay
 * -----
 */

#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

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
#ifdef _DEBUG
    log_set_level(gerber_lib::log_level_debug);
#else
    log_set_level(gerber_lib::log_level_warning);
#endif

#ifdef _WIN32
    if(!IsDebuggerPresent()) {
        if(AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE *dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
            freopen_s(&dummy, "CONIN$", "r", stdin);
        }
        gerber_lib::log_set_emitter_function(puts);
    } else {
        gerber_lib::log_set_emitter_function(flushed_puts);
    }
#else
    gerber_lib::log_set_emitter_function(puts);
#endif

    gerber_explorer window;
    window.init();

    while(window.update()) {}

    return 0;
}
