#pragma once

#include <array>
#include "zlth_dsp_filter.h"
#include "config.h"
#include "zlth_simd.h"

template <int BandCount>
class ProcessChain {
public:
  FI void process(std::span<float> span) {
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
