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

    template <size_t reserve_size = 1ULL << 30, size_t alignment = 8, size_t min_grow_size = 16 * 1024> struct gerber_arena
    {
        LOG_CONTEXT("arena", error);

        //////////////////////////////////////////////////////////////////////

        gerber_arena() : used_size(0), committed_size(0)
        {
            base_address = allocate_address_space(reserve_size);
        }

        //////////////////////////////////////////////////////////////////////

        ~gerber_arena()
        {
            release();
        }

        //////////////////////////////////////////////////////////////////////

        gerber_arena(const gerber_arena&) = delete;
        gerber_arena& operator=(const gerber_arena&) = delete;

        //////////////////////////////////////////////////////////////////////

        void *alloc(size_t size)
        {
            uintptr_t current_ptr = reinterpret_cast<uintptr_t>(base_address) + used_size;
            used_size += (alignment - (current_ptr % alignment)) % alignment;

            if(used_size + size > reserve_size) {
                return nullptr;
            }

            void *ptr = base_address + used_size;
            used_size += size;

            if(used_size > committed_size) {
                size_t need_to_commit = used_size - committed_size;
                size_t commit_size = (std::max(min_grow_size, need_to_commit) + system_page_size - 1) & ~(system_page_size - 1);

                if(!commit_address_space(base_address + committed_size, commit_size)) {
                    return nullptr;
                }
                committed_size += commit_size;
            }
            return ptr;
        }

        //////////////////////////////////////////////////////////////////////

        void reset()
        {
            used_size = 0;
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

        std::byte *base_address;
        size_t used_size;
        size_t committed_size;
    };

    //////////////////////////////////////////////////////////////////////
    // Helper for using it as a one off std::vector replacement

    template <typename U, size_t reserve_size = 1ULL << 30, size_t alignment = alignof(U), size_t min_grow_size = 16 * 1024>
    struct typed_arena : gerber_arena<reserve_size, alignment, min_grow_size>
    {
        using value_type = U;

        //////////////////////////////////////////////////////////////////////

        typed_arena() : gerber_arena<reserve_size, alignment, min_grow_size>::gerber_arena(), count(0)
        {
        }

        //////////////////////////////////////////////////////////////////////

        typed_arena(const typed_arena&) = delete;
        typed_arena& operator=(const typed_arena&) = delete;

        //////////////////////////////////////////////////////////////////////

        void push_back(U const &item)
        {
            U *p = reinterpret_cast<U *>(this->alloc(sizeof(U)));
            *p = item;
            count += 1;
        }

        //////////////////////////////////////////////////////////////////////

        template <typename... Args> void emplace_back(Args &&...args)
        {
            U *p = reinterpret_cast<U *>(this->alloc(sizeof(U)));
            new(p) U(std::forward<Args>(args)...);
            count += 1;
        }

        //////////////////////////////////////////////////////////////////////

        void pop_back()
        {
            count -= 1;
        }

        //////////////////////////////////////////////////////////////////////

        U *data()
        {
            return reinterpret_cast<U *>(this->base_address);
        }

        //////////////////////////////////////////////////////////////////////

        U const *data() const
        {
            return reinterpret_cast<U *>(this->base_address);
        }

        //////////////////////////////////////////////////////////////////////

        U & operator[](size_t offset)
        {
            // assert(offset < count);
            return data()[offset];
        }

        //////////////////////////////////////////////////////////////////////

        U const & operator[](size_t offset) const
        {
            // assert(offset < count);
            return data()[offset];
        }

        //////////////////////////////////////////////////////////////////////

        size_t size() const
        {
            return count;
        }

        //////////////////////////////////////////////////////////////////////

        U *begin()
        {
            return data();
        }

        //////////////////////////////////////////////////////////////////////

        U *end()
        {
            return data() + size();
        }

        //////////////////////////////////////////////////////////////////////

        U &back()
        {
            // assert(count != 0);
            return data()[size() - 1];
        }

        //////////////////////////////////////////////////////////////////////

        U &front()
        {
            // assert(count != 0);
            return data()[0];
        }

        //////////////////////////////////////////////////////////////////////

        bool empty() const
        {
            return size() == 0;
        }

        //////////////////////////////////////////////////////////////////////

        void increase_size_to(size_t n)
        {
            if(n > count) {
                // does NOT call any constructors!!!!!!!!!!
                this->alloc(sizeof(value_type) * (n - count));
                count = n;
            }
        }

        //////////////////////////////////////////////////////////////////////

        void clear()
        {
            this->reset();
            count = 0;
        }

        size_t count;
    };

}    // namespace gerber_lib
