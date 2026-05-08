#pragma once

#include <array>
#include "forceinline.h"
#include "zlth_simd.h"

namespace zlth::dsp {
  class Gain {
  public:

    FORCEINLINE void set_gain(float tp_) noexcept {
      lerp_state = true;
      tp = tp_;
    }

    FORCEINLINE void process(std::span<float> span) noexcept {
      if (std::exchange(lerp_state, false)) {
        const size_t numSamples = span.size();
        const float deltaDiff = (tp - cp) / static_cast<float>(numSamples);
        for (size_t i = 0; i < numSamples; ++i) {
          cp += deltaDiff;
          span[i] *= cp;
        }
        cp = tp;
      }
      else {
        zlth::simd::mul_inplace(span, cp);
      }
    }

  private:
    float tp {1.0f};
    float cp {1.0f};
    bool lerp_state {};
  };
}
