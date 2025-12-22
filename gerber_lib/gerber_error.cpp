//////////////////////////////////////////////////////////////////////

#include "gerber_error.h"
#include <map>

namespace
{
#undef GERBER_ERROR_CODES
#undef GERBER_ERROR_CODE
#define GERBER_ERROR_CODE(a) { gerber_lib::error_##a, #a },
#include "gerber_error_codes.h"

    std::map<uint32_t, char const *> error_names_map = { { gerber_lib::ok, "ok" }, GERBER_ERROR_CODES };
}    // namespace

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    char const *get_error_text(gerber_error_code error_code)
    {
        auto f = error_names_map.find(static_cast<uint32_t>(error_code));
        if(f == error_names_map.end()) {
            return "?";
        }
        return f->second;
    }
}    // namespace gerber_lib