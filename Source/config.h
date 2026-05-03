#pragma once

#include <cmath>
#include <numbers>

namespace config {

  inline const juce::StringArray filterModeStrings {
    "HIGHPASS",
    "LOWPASS",
    "HIGHSHELF",
    "LOWSHELF",
    "TILT", 
    "BELL",
    "NOTCH",
    "BANDPASS"
  };
  inline const juce::StringArray channelModeStrings {
    "STEREO",
    "MID",
    "SIDE"
  };
  inline const juce::StringArray ButterShapeStrings {
    "Flat",
    "Smooth",
    "Sharp"
  };
  enum class FilterType {
    HighPass,
    LowPass,
    HighShelf,
    LowShelf,
    Tilt,
    Bell,
    Notch,
    BandPass,
    PassThrough
  };
  enum class ChannelMode {
    Stereo,
    Mid,
    Side
  };
  enum class ButterType {
    Flat,
    Smooth,
    Sharp
  };

  inline const juce::String ID_Q {"Q"};
  inline const juce::String ID_FREQ {"FREQ"};
  inline const juce::String ID_GAIN {"GAIN"};
  inline const juce::String ID_ORDER {"ORDER"};
  inline const juce::String ID_FILTER_SHAPE {"FILTER_MODE"};
  inline const juce::String ID_BUTTER_SHAPE {"BUTTER_MODE"};
  inline const juce::String ID_CHANNEL_MODE {"CHANNEL_MODE"};
  inline const juce::String ID_BYPASS {"BYPASS"};

  inline const juce::String UNIT_Q {"Q"};
  inline const juce::String UNIT_FREQ {"Hz"};
  inline const juce::String UNIT_GAIN {"dB"};
  inline const juce::String UNIT_ORDER {"N"};

  inline constexpr float PARAM_Q_MIN {1.0f / 16.0f};
  inline constexpr float PARAM_Q_MAX {16.0f};
  inline constexpr float PARAM_Q_DEF {1.0f / std::numbers::sqrt2_v<float>};

  inline constexpr float PARAM_FREQ_HZ_MIN {20.0f};
  inline constexpr float PARAM_FREQ_HZ_MAX {20000.0f};

  inline constexpr float PARAM_GAIN_DB_MIN {-24.0f};
  inline constexpr float PARAM_GAIN_DB_MAX {24.0f};
  inline constexpr float PARAM_GAIN_DB_DEF {0.0f};

  inline constexpr int PARAM_ORDER_MIN {1};
  inline constexpr int PARAM_ORDER_MAX {6};
  inline constexpr int PARAM_ORDER_DEF {6};

  inline constexpr int PARAM_FILTER_DEFAULT {(int)FilterType::Bell};
  inline constexpr int PARAM_BUTTER_DEFAULT {(int)ButterType::Flat};
  inline constexpr int PARAM_CHANNEL_DEFAULT {(int)ChannelMode::Stereo};
  inline constexpr bool PARAM_BYPASS_DEFAULT {true};

  inline constexpr int CHANNEL_COUNT {2};
  //-------2ndOrderSVFIDs
  inline constexpr int BIQUAD_COUNT {8};
  inline constexpr int biquadPrefixCount {6};
  inline const std::array<juce::String, biquadPrefixCount> biquadPrefixes {ID_FREQ, ID_GAIN, ID_Q, ID_FILTER_SHAPE, ID_BYPASS, ID_CHANNEL_MODE};
  inline const std::array<juce::String, 3> bandUnits {UNIT_GAIN, UNIT_FREQ, UNIT_Q};
  enum class BandAddressEnum {
    freq,
    gain,
    q,
    shape,
    bypass,
    channel
  };
  //-------BUTTERWORTHIDs
  inline constexpr int butterPrefixCount {5};
  inline constexpr int BUTTER_COUNT {2};
  inline const std::array<juce::String, BUTTER_COUNT> butterworthLabels {"LOWPASS", "HIGHPASS"};
  inline const std::array<FilterType, BUTTER_COUNT> ButterFilterTypeDef {FilterType::HighPass, FilterType::LowPass};
  inline const std::array<juce::String, butterPrefixCount> butterPrefixes {ID_FREQ, ID_ORDER, ID_BYPASS, ID_BUTTER_SHAPE, ID_CHANNEL_MODE};
  inline const std::array<juce::String, 2> butterUnits {UNIT_FREQ, UNIT_ORDER};
  enum class ButterAddressEnum {
    freq,
    order,
    bypass,
    shape,
    channel
  };
  //-------OtherIDs
  inline const juce::String ID_PARAMETERS {"PARAMETERS"};
  inline const std::array<juce::String, CHANNEL_COUNT> ID_OUT_GAIN {"OUT_GAIN_MID", "OUT_GAIN_SIDE"};

  inline const std::array<juce::String, 3> modeNames {"STEREO", "MID", "SIDE"};
  inline const std::array<juce::String, 2> masterGainLabels {"MID", "SIDE"};

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

  inline juce::String IndexToID(int index) {
    return juce::String(index + 1);
  }

  inline juce::String toBiquadID(const juce::String& prefix, int index) {
    return prefix + juce::String(index + 1);
  }
  inline int toIndex(const juce::String& parameterID) {
    return parameterID.getTrailingIntValue() - 1;
  }
  inline juce::String toButterID(const juce::String& prefix, int index) {
    return juce::String("Butterworth") + prefix + juce::String(index + 1);
  }

  inline constexpr std::array<float, 9> editorDBs {24.0f, 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f};
  inline constexpr std::array<float, 9> meterDBs {12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f};
  inline constexpr std::array<float, 10> frequencies {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
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
  inline const juce::Colour initialize {0xffff0000};
  inline const juce::Colour sliderPointer {0xffffffff};
  inline const juce::Colour buttonDisabled {0xff555555};
  inline const juce::Colour textBackground {0xff000000};
  inline const juce::Colour pluginBackground {0xff202020};
}
