#include <cstdio>
#include <glad/glad.h>

#include "gerber_lib.h"
#include "gerber_explorer.h"

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
