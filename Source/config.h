#pragma once

#include <cmath>
#include <numbers>

namespace config {
   const std::array<float, 9> editorDBs {24.0f, 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f};
   const std::array<float, 9> meterDBs {12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f};
   const std::array<float, 9> fftDBs {30.0f, 15.0f, 0.0f, -15.0f, -30.0f, -45.0f, -60.0f, -75.0f, -90.0f};
   const std::array<float, 10> frequencies {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};

   inline constexpr int BAND_COUNT {8};

   inline constexpr bool PARAM_BYPASS_DEFAULT {true};
   inline constexpr int PARAM_CHANNEL_DEFAULT {0};
   inline constexpr int PARAM_FILTER_DEFAULT {4};

   inline constexpr float PARAM_FREQ_MIN {20.0f};
   inline constexpr float PARAM_FREQ_MAX {20000.0f};
   inline constexpr float PARAM_FREQ_DEF {500.0f};
   inline constexpr float PARAM_QUAL_MIN {1.0f / 16.0f};
   inline constexpr float PARAM_QUAL_MAX {16.0f};
   inline constexpr float PARAM_QUAL_DEF {1.0f / std::numbers::sqrt2_v<float>};
   inline constexpr float PARAM_GAIN_MIN {-24.0f};
   inline constexpr float PARAM_GAIN_MAX {24.0f};
   inline constexpr float PARAM_GAIN_DEF {0.0f};

   inline constexpr float METER_MIN {-36.0f};
   inline constexpr float METER_MAX {12.0f};
   inline constexpr float FFT_MIN_DB {-90.0f};
   inline constexpr float FFT_MAX_DB {30.0f};

   inline const juce::Colour red {0xffff2020};
   inline const juce::Colour side {0xffff3d67};
   inline const juce::Colour text {0xffffffff};
   inline const juce::Colour theme {0xff4284ff};
   inline const juce::Colour slider {0xff181818};
   inline const juce::Colour groove {0xff101010};
   inline const juce::Colour sliderRim {0xff505050};
   inline const juce::Colour sliderPointer {0xffffffff};
   inline const juce::Colour buttonDisabled {0xff555555};
   inline const juce::Colour textBackground {0xff000000};
   inline const juce::Colour pluginBackground {0xff202020};

   inline const juce::String ID_PARAMETERS {"PARAMETERS"};
   inline const juce::String ID_OUT_GAIN_0 {"OUT_GAIN_MID"};
   inline const juce::String ID_OUT_GAIN_1 {"OUT_GAIN_SIDE"};
   inline const juce::String ID_BAND_FREQ {"FREQ"};
   inline const juce::String ID_BAND_GAIN {"GAIN"};
   inline const juce::String ID_BAND_QUAL {"Q"};
   inline const juce::String ID_BAND_BYPASS {"BYPASS"};
   inline const juce::String ID_BAND_FILTER {"FILTER_MODE"};
   inline const juce::String ID_BAND_CHANNEL {"CHANNEL_MODE"};
   inline const juce::StringArray channelModes {"STEREO", "MID", "SIDE"};
   inline const juce::StringArray filterModes {"HIGHPASS", "LOWPASS", "HIGHSHELF", "LOWSHELF", "TILT", "BELL", "NOTCH", "BANDPASS"};
   inline const juce::StringArray bandParamPrefixes {ID_BAND_FREQ, ID_BAND_GAIN, ID_BAND_QUAL, ID_BAND_FILTER, ID_BAND_BYPASS, ID_BAND_CHANNEL};

   inline const std::array<juce::String, 3> modeNames {"STEREO", "MID", "SIDE"};
   inline const std::array<juce::String, 2> masterGainLabels {"MID", "SIDE"};
   inline const std::array<juce::String, 3> bandUnits {"dB", "Hz", "Q"};

   inline constexpr int iconCount {8};
   inline const std::array<const char*, iconCount> iconData {
      BinaryData::hp_svg,
      BinaryData::lp_svg,
      BinaryData::hs_svg,
      BinaryData::ls_svg,
      BinaryData::tilt_svg,
      BinaryData::peak_svg,
      BinaryData::notch_svg,
      BinaryData::bp_svg,
   };
   inline const std::array<const int, iconCount> iconSize {
      BinaryData::hp_svgSize,
      BinaryData::lp_svgSize,
      BinaryData::hs_svgSize,
      BinaryData::ls_svgSize,
      BinaryData::tilt_svgSize,
      BinaryData::peak_svgSize,
      BinaryData::notch_svgSize,
      BinaryData::bp_svgSize
   };

   inline juce::String getID(const juce::String& prefix, int bandIdx) {
      return prefix + juce::String(bandIdx + 1);
   }
   inline juce::String IndexToID(int bandIdx) {
      return juce::String(bandIdx + 1);
   }
   inline int getBandIndex(const juce::String& parameterID) {
      return parameterID.getTrailingIntValue() - 1;
   }
}
