#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include "forceinline.h"

namespace zlth::unit {
  inline constexpr float ln10_div_20 {std::numbers::ln10_v<float> / 20.0f};
  inline constexpr float ln10_div_40 {std::numbers::ln10_v<float> / 40.0f};
  FI inline float dbToMag(float value) {
    return std::exp(ln10_div_20 * value);
  }
  FI inline float dbToMagSqrt(float value) {
    return std::exp(ln10_div_40 * value);
  }
  FI inline float magToDB(float value, float min = 1e-20f) {
    return 20.0f * std::log10(std::max(value, min));
  }
  FI inline float magSqToDB(float value, float min = 1e-10f) {
    return 10.0f * std::log10(std::max(value, min));
  }
}
