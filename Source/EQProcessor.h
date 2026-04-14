#pragma once

#include <array>
#include "zlth_dsp_filter.h"
#include "config.h"
#include "zlth_simd.h"

#if defined(_MSC_VER)
#define ZLTH_FORCEINLINE [[msvc::forceinline]]
#elif defined(__GNUC__) || defined(__clang__)
#define ZLTH_FORCEINLINE __attribute__((always_inline)) inline
#else
#define ZLTH_FORCEINLINE inline
#endif

template <int BandCount>
class ProcessChain {
public:
   ZLTH_FORCEINLINE void process(std::span<float> span) {
      zlth::simd::mul_inplace(span, globalGain);
      for (int i = 0; i < BandCount; ++i) {
         if (isBandActive[i]) {
            bands[i].process_span(span);
         }
      }
   }
   float globalGain {};
   std::array<zlth::dsp::Filter, BandCount> bands {};
   std::array<bool, BandCount> isBandActive {};
};
