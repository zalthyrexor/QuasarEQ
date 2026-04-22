#pragma once

#if defined(_MSC_VER)
#define FORCEINLINE [[msvc::forceinline]]
#elif defined(__GNUC__) || defined(__clang__)
#define FORCEINLINE __attribute__((always_inline)) inline
#else
#define FORCEINLINE inline
#endif
