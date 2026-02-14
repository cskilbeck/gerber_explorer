#include <gerber_util.h>
#include <algorithm>
#include <codecvt>
#include <charconv>

namespace gerber_util
{
    //////////////////////////////////////////////////////////////////////

    std::string to_lowercase(std::string const &s)
    {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return r;
    }

    //////////////////////////////////////////////////////////////////////

    std::expected<double, ParseError> double_from_string_view(std::string_view sv)
    {
        double value{};

        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

        if(ec == std::errc::invalid_argument) {
            return std::unexpected(ParseError::InvalidInput);
        }
        if(ec == std::errc::result_out_of_range) {
            return std::unexpected(ParseError::OutOfRange);
        }
        if(ptr != sv.data() + sv.size()) {
            return std::unexpected(ParseError::InvalidInput);
        }
        return value;
    }
}    // namespace gerber_util
