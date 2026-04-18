#pragma once

#if defined(_MSC_VER)
#define FI [[msvc::forceinline]]
#elif defined(__GNUC__) || defined(__clang__)
#define FI __attribute__((always_inline)) inline
#else
#define FI inline
#endif
