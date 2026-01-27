#pragma once

#if defined(_MSC_VER)
    // Microsoft Visual C++
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    // GCC or Clang
    #define FORCEINLINE inline __attribute__((always_inline))
#else
    // Fallback for other compilers
    #define FORCEINLINE inline
#endif
