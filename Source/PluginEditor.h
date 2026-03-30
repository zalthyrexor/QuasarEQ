#pragma once

#include "custom_components.h"

struct IconData { const char* data; int size; };

class QuasarEQAudioProcessorEditor: public juce::AudioProcessorEditor
{
public:
    QuasarEQAudioProcessorEditor(QuasarEQAudioProcessor& p);
    ~QuasarEQAudioProcessorEditor();
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    std::vector<IconData> icons {
    {BinaryData::hp_svg, BinaryData::hp_svgSize},
    {BinaryData::lp_svg, BinaryData::lp_svgSize},
    {BinaryData::hs_svg, BinaryData::hs_svgSize},
    {BinaryData::ls_svg, BinaryData::ls_svgSize},
    {BinaryData::peak_svg, BinaryData::peak_svgSize},
    {BinaryData::tilt_svg, BinaryData::tilt_svgSize},
    {BinaryData::notch_svg, BinaryData::notch_svgSize},
    {BinaryData::bp_svg, BinaryData::bp_svgSize}
    };
    static constexpr int margin = 4;
    static constexpr int sectionAHeight = 30;
    static constexpr int sectionBHeight = 42;
    static constexpr int sectionCHeight = 300;
    static constexpr int sectionDHeight = 300+30;
    static constexpr int windowHeight = margin * 2 + sectionAHeight + sectionBHeight + sectionCHeight + sectionDHeight;
    static constexpr int windowWidth = 657;
    CustomLNF customLNF;
    QuasarEQAudioProcessor& audioProcessor;
    VisualizerComponent visualizerComponent;
    juce::Label pluginInfoLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    static auto getMasterGainIDs() -> const std::array<juce::String, 2>&
    {
        static const std::array<juce::String, 2> ids {
            ID_OUT_GAIN_MID, ID_OUT_GAIN_SIDE
        };
        return ids;
    }
    static inline const std::array<juce::String, 2> masterGainLabels {
        "M", "S"
    };
    std::array<juce::Slider, 2> masterGainSliders;
    std::array<std::unique_ptr<juce::Label>, 2> masterGainLabelsComponents;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> masterGainAttachments;

    std::vector<std::unique_ptr<FilterBandControl>> bandControls;
    std::vector<std::unique_ptr<CustomIconButton>> paletteButtons;
    int selectedFilterType = 4;
};
