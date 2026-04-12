#pragma once

#include <cmath>
#include <numbers>

namespace config
{
    inline constexpr int BAND_COUNT = 8;
    inline constexpr int OUT_GAIN_COUNT = 1;

    inline constexpr float PARAM_BAND_FREQ_MIN = 20.0f;
    inline constexpr float PARAM_BAND_FREQ_MAX = 20000.0f;
    inline const float PARAM_BAND_FREQ_CENTER = sqrt(PARAM_BAND_FREQ_MIN * PARAM_BAND_FREQ_MAX);
    inline constexpr float PARAM_BAND_FREQ_INTERVAL = 0.1f;

    inline constexpr float PARAM_BAND_GAIN_MIN = -24.0f;
    inline constexpr float PARAM_BAND_GAIN_MAX = 24.0f;
    inline constexpr float PARAM_BAND_GAIN_CENTER = 0.0f;
    inline constexpr float PARAM_BAND_GAIN_INTERVAL = 0.01f;

    inline constexpr float PARAM_BAND_QUAL_MIN = 0.05f;
    inline constexpr float PARAM_BAND_QUAL_MAX = 12.0f;
    inline constexpr float PARAM_BAND_QUAL_CENTER = 1.0f / std::numbers::sqrt2_v<float>;
    inline constexpr float PARAM_BAND_QUAL_INTERVAL = 0.001f;

    inline constexpr bool PARAM_BAND_BYPASS_DEFAULT = true;
    inline constexpr int PARAM_BAND_CHANNEL_DEFAULT = 0;
    inline constexpr int PARAM_BAND_FILTER_DEFAULT = 4;

    inline constexpr float PARAM_OUT_GAIN_MIN = -24.0f;
    inline constexpr float PARAM_OUT_GAIN_MAX = 24.0f;
    inline constexpr float PARAM_OUT_GAIN_CENTER = 0.0f;
    inline constexpr float PARAM_OUT_GAIN_INTERVAL = 1.0f;

    inline constexpr float METER_MIN = -36.0f;
    inline constexpr float METER_MAX = 12.0f;
    inline constexpr float FFT_MIN_DB = -90.0f;
    inline constexpr float FFT_MAX_DB = 30.0f;

    inline constexpr std::string_view UNIT_HZ {"Hz"};
    inline constexpr std::string_view UNIT_DB {"dB"};

    inline const juce::Colour theme {0xff4284ff};
    inline const juce::Colour groove {0xff101010};
    inline const juce::Colour buttonDisabled {0xff555555};
    inline const juce::Colour text {0xffffffff};
    inline const juce::Colour textBackground {0xff000000};
    inline const juce::Colour pluginBackground {0xff202020};
    inline const juce::Colour slider {0xff181818};
    inline const juce::Colour sliderRim {0xff505050};
    inline const juce::Colour sliderPointer {0xffffffff};
    inline const juce::Colour side {0xffff3d67};
}
