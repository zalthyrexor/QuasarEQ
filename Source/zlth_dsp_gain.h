#pragma once

#include <array>
#include "forceinline.h"
#include "zlth_simd.h"
#include "IProcessor.h"

namespace zlth::dsp {
  class Gain: public IProcessor {
  public:
    FORCEINLINE Gain(std::atomic<float>* g): gainParam(g) {
    }
    FORCEINLINE void process(std::initializer_list<std::span<float>> spans) noexcept {
      const float tp = zlth::unit::dbToMag(gainParam->load());
      for (auto span : spans) {
        float cp_ = cp;
        if (tp == cp_) {
          zlth::simd::mul_inplace(span, cp_);
        }
        else {
          const size_t numSamples = span.size();
          const float deltaDiff = (tp - cp_) / static_cast<float>(numSamples);
          for (size_t i = 0; i < numSamples; ++i) {
            span[i] *= cp_;
            cp_ += deltaDiff;
          }
        }
      }
      cp = tp;
    }
  private:
    float cp {1.0f};
    std::atomic<float>* gainParam;
  };
}
