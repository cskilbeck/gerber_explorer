#pragma once

//////////////////////////////////////////////////////////////////////

#include "gerber_log.h"

//////////////////////////////////////////////////////////////////////

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    bool commit_address_space(void *addr, size_t size);
    std::byte *allocate_address_space(size_t size);
    void deallocate_address_space(void *p);

    extern size_t system_page_size;

    //////////////////////////////////////////////////////////////////////

    template <size_t reserve_size = 1ULL << 32, size_t alignment = 8, size_t min_grow_size = 16 * 1024> struct gerber_arena
    {
        LOG_CONTEXT("arena", error);

        //////////////////////////////////////////////////////////////////////

        gerber_arena() : used_size(0), committed_size(0), user_data(0)
        {
            base_address = allocate_address_space(reserve_size);
        }

        //////////////////////////////////////////////////////////////////////

        ~gerber_arena()
        {
            release();
        }

        //////////////////////////////////////////////////////////////////////

        void *alloc(size_t size)
        {
            uintptr_t current_ptr = reinterpret_cast<uintptr_t>(base_address) + used_size;
            uintptr_t padding = (alignment - (current_ptr % alignment)) % alignment;
            size_t total_size = size + padding;

            if(used_size + total_size + padding > reserve_size) {
                LOG_ERROR("OUT OF SPACE! Need {}, got {}", used_size + total_size, reserve_size);
                return nullptr;
            }

            void *ptr = base_address + used_size + padding;
            used_size += total_size;

            if(used_size <= committed_size) {
                return ptr;
            }

            size_t commit_size = std::max(min_grow_size, used_size - committed_size);
            commit_size = (commit_size + system_page_size - 1) & ~(system_page_size - 1);

            if(!commit_address_space(base_address + committed_size, commit_size)) {
                LOG_ERROR("Can't commit! Need {}", commit_size);
                return nullptr;
            }
            committed_size += commit_size;
            return ptr;
        }

        //////////////////////////////////////////////////////////////////////

        void reset()
        {
            used_size = 0;
            user_data = 0;
        }

        //////////////////////////////////////////////////////////////////////

        void release()
        {
            deallocate_address_space(base_address);
        }

        //////////////////////////////////////////////////////////////////////

        double percent_committed()
        {
            return (double)committed_size / reserve_size * 100.0;
        }

        //////////////////////////////////////////////////////////////////////
        // Helper for using it as a one off std::vector replacement

        template<typename U> void push_back(U const &item)
        {
            (U *)alloc(sizeof(U)) = item;
            user_data += 1;
        }

        //////////////////////////////////////////////////////////////////////
        // janky: arena.emplace_back<mything>(...);

        template <typename T, typename... Args> void emplace_back(Args&&... args)
        {
            new ((T *)alloc(sizeof(T))) T(std::forward<Args>(args)...);
            user_data += 1;
        }

        //////////////////////////////////////////////////////////////////////

        std::byte *base_address;
        size_t used_size;
        size_t committed_size;
        size_t user_data;
    };

}    // namespace gerber_lib
