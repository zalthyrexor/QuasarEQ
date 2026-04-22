#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include "forceinline.h"

namespace zlth::unit {
  inline constexpr float ln10_div_20 {std::numbers::ln10_v<float> / 20.0f};
  inline constexpr float ln10_div_80 {std::numbers::ln10_v<float> / 80.0f};
  FORCEINLINE inline float dbToMag(float value) {
    return std::exp(ln10_div_20 * value);
  }
  FORCEINLINE inline float dbToMagFourthRoot(float value) {
    return std::exp(ln10_div_80 * value);
  }
  FORCEINLINE inline float magToDB(float value, float min = 1e-20f) {
    return 20.0f * std::log10(std::max(value, min));
  }
  FORCEINLINE inline float magSqToDB(float value, float min = 1e-10f) {
    return 10.0f * std::log10(std::max(value, min));
  }
}
