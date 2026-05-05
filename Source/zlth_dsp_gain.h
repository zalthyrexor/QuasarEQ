#pragma once

#include <array>
#include "forceinline.h"
#include "zlth_simd.h"
#include "IProcessor.h"

namespace zlth::dsp {
  class Gain {
  public:
    FORCEINLINE Gain(std::atomic<float>* g): gainParam(g) {
    }
    FORCEINLINE void process(std::span<float> span) noexcept {
      const float tp = zlth::unit::dbToMag(gainParam->load());
      if (tp == cp) {
        zlth::simd::mul_inplace(span, cp);
      }
      else {
        const size_t numSamples = span.size();
        const float deltaDiff = (tp - cp) / static_cast<float>(numSamples);
        for (size_t i = 0; i < numSamples; ++i) {
          span[i] *= cp;
          cp += deltaDiff;
        }
        cp = tp;
      }
    }
  private:
    float cp {1.0f};
    std::atomic<float>* gainParam;
  };
}
