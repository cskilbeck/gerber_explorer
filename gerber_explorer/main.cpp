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
// X thick arc shader (but turns out we can't use it)
// X render-on-window-resize
// X fix ImGui viewport thing
// X picking / selection
// X detect gerber layer type/position from extension/name/x2 info
// X top/bottom views
// X selected entity info
// X isolate / unisolate layer
// X tidy up the cmake
// X reduce gpu usage - only draw frame when necessary
// X fix blending / inverted layers / alpha
// X allow user to specify board outline layer dynamically (e.g. ko from jlcpcb)
// X fix broken tesselation
// X mm/inches
// X fix job pool hang
// X don't save settings if layer loading in progress
// X there can be only one outline layer
// X detect & use board outline for inverted layers
// X show icon for outline layer
//
// fix select/hover/active highlighting
// make the gerber parser interruptible with stop_token
// share arenas where possible in gl_drawer (pool of arenas reused?)
// make status bar more informative
// use native menus on MacOS
// dynamic tesselation
// measure tool
// fix the memory allocation - especially for tesselator and gerber::draw
//
// gerber spec compliance tests (regions, polygons, holes)
//
// settings window?
// configurable mouse buttons (/keys?)
// 3D view
// high DPI
// grid
// load zip file? (how to store path in settings?)
// export PNG
// fork/mirror 3rd party repos
// ? OpenCascade 3D nonsense ?
// ? undo/redo ?
//

#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
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

#ifdef _WIN32
int output_debug_string(char const *s)
{
    OutputDebugStringA(s);
    OutputDebugStringA("\n");
    return 0;
}
#endif

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
#if defined(LOG_USE_OUTPUT_DEBUG_STRING)
        gerber_lib::log_set_emitter_function(output_debug_string);
#else
        gerber_lib::log_set_emitter_function(flushed_puts);
#endif
    }
#else
    gerber_lib::log_set_emitter_function(puts);
#endif

    gerber_explorer window;
    window.init();

    while(window.update()) {}

    return 0;
}
