#include <gerber_util.h>
#include <algorithm>

namespace
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter{};

}    // namespace

namespace gerber_util
{
    //////////////////////////////////////////////////////////////////////

    std::wstring utf16_from_utf8(std::string const &s)
    {
        return converter.from_bytes(s);
    }

    //////////////////////////////////////////////////////////////////////

    std::string to_lowercase(std::string const &s)
    {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return r;
    }

}    // namespace gerber_util
