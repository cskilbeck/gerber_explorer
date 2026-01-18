//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "gerber_log.h"
#include "gerber_arena.h"

LOG_CONTEXT("arena", debug);

namespace gerber_lib
{
#ifdef _WIN32

    //////////////////////////////////////////////////////////////////////

    size_t get_page_size()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwPageSize;
    }

    //////////////////////////////////////////////////////////////////////

    bool commit_address_space(void *addr, size_t size)
    {
        return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
    }

    //////////////////////////////////////////////////////////////////////

    std::byte *allocate_address_space(size_t size)
    {
        return (std::byte *)VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    }

    //////////////////////////////////////////////////////////////////////

    void deallocate_address_space(void *p)
    {
        if(p != nullptr) {
            VirtualFree(p, 0, MEM_RELEASE);
        }
    }

#else

    //////////////////////////////////////////////////////////////////////

    size_t get_page_size()
    {
        return sysconf(_SC_PAGESIZE);
    }

    //////////////////////////////////////////////////////////////////////

    bool commit_address_space(void *addr, size_t size)
    {
        return mprotect(addr, size, PROT_READ | PROT_WRITE) == 0;
    }

    //////////////////////////////////////////////////////////////////////

    std::byte *allocate_address_space(size_t size)
    {
        return (std::byte *)mmap(nullptr, reserve_size_, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    //////////////////////////////////////////////////////////////////////

    void deallocate_address_space(void *p)
    {
        if(p != nullptr) {
            munmap(p, reserve_size_);
        }
    }
#endif

    size_t system_page_size = get_page_size();

}    // namespace gerber_lib
