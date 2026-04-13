#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zlth::unit
{
	inline constexpr float ln10_div_20 {std::numbers::ln10_v<float> / 20.0f};
	inline constexpr float ln10_div_40 {std::numbers::ln10_v<float> / 40.0f};

	inline float dbToMag(float db) {
		return std::exp(ln10_div_20 * db);
	}
	inline float dbToMagSqrt(float db) {
		return std::exp(ln10_div_40 * db);
	}
	inline float magToDB(float mag, float min = 1e-20f) {
		return 20.0f * std::log10(std::max(mag, min));
	}
	inline float magSqToDB(float magSq, float min = 1e-10f) {
		return 10.0f * std::log10(std::max(magSq, min));
	}
}
