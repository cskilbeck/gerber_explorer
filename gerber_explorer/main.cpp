#include <cstdio>
#include "gerber_lib.h"
#include "log_drawer.h"

using gerber_lib::gerber_error_code;

int main(int, char **)
{
    log_set_level(gerber_lib::log_level_debug);
    gerber_lib::log_set_emitter_function(puts);
    gerber_lib::gerber g;
    g.parse_file("../../gerber_test_files/2-13-1_Two_square_boxes.gbr");
    gerber_3d::log_drawer d;
    d.set_gerber(&g);
    g.draw(d);
}
