//////////////////////////////////////////////////////////////////////

#include "gerber_stats.h"
#include "gerber_aperture.h"

LOG_CONTEXT("stats", debug);

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    void gerber_stats::cleanup()
    {
        errors.clear();
        apertures.clear();
        d_codes.clear();
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_stats::add_aperture(int level, int number, gerber_aperture_type type, double parameter[5])
    {
        for(auto const a : apertures) {
            if(a->number == number && a->aperture_type == type) {
                return;
            }
        }

        // This aperture number is unique.  Therefore, add it to the list.
        // Debug.WriteLine("    Adding type {0} to aperture list ", type);

        gerber_aperture_info *aperture = new gerber_aperture_info();

        apertures.emplace_back(aperture);

        aperture->level = level;
        aperture->number = number;
        aperture->aperture_type = type;
        memcpy(aperture->parameters, parameter, sizeof(double) * 5);
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_stats::add_to_d_list(int number)
    {
        for(auto const d : d_codes) {
            if(d->number == number) {
                return;
            }
        }
        gerber_aperture_info *d = new gerber_aperture_info();
        d_codes.emplace_back(d);

        // This aperture number is unique, add it to the list.
        // Debug.WriteLine("    Adding code {0} to D List", number);
        d->number = number;
        d->count = 0;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_stats::increment_d_list_count(int number, int count, int line)
    {
        (void)count;
        (void)line;

        for(auto d : d_codes) {
            if(d->number == number) {
                d->count += 1;
                return ok;
            }
        }
        return error_undefined_d_code;
    }

    //////////////////////////////////////////////////////////////////////

    void gerber_stats::add_new_d_list(int number)
    {
        for(gerber_aperture_info *info : d_codes) {
            if(info->number == number) {
                LOG_DEBUG("Code {} already exists in D list", number);
                return;
            }
        }
        gerber_aperture_info *new_d_code = new gerber_aperture_info{};
        new_d_code->number = number;
        new_d_code->count = 0;
        d_codes.push_back(new_d_code);
    }

}    // namespace gerber_lib