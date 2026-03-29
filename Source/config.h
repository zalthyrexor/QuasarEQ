#pragma once

#include <cmath>
#include <numbers>

namespace config
{
    inline constexpr int BAND_COUNT = 8;

    inline constexpr float PARAM_FREQ_MIN = 20.0f;
    inline constexpr float PARAM_FREQ_MAX = 20000.0f;
    inline const float PARAM_FREQ_CENTER = sqrt(PARAM_FREQ_MIN * PARAM_FREQ_MAX);
    inline constexpr float PARAM_FREQ_INTERVAL = 0.1f;

    inline constexpr float PARAM_GAIN_MIN = -24.0f;
    inline constexpr float PARAM_GAIN_MAX = 24.0f;
    inline constexpr float PARAM_GAIN_CENTER = 0.0f;
    inline constexpr float PARAM_GAIN_INTERVAL = 0.01f;

    inline constexpr float PARAM_QUAL_MIN = 0.05f;
    inline constexpr float PARAM_QUAL_MAX = 12.0f;
    inline constexpr float PARAM_QUAL_CENTER = 1.0f / std::numbers::sqrt2_v<float>;
    inline constexpr float PARAM_QUAL_INTERVAL = 0.001f;

    inline constexpr bool PARAM_BYPASS_DEFAULT = true;
    inline constexpr int PARAM_MODE_DEFAULT = 0;
    inline constexpr int PARAM_TYPE_DEFAULT = 4;
}
