#pragma once

#include "VisualizerComponent.h"

class CustomButton: public juce::Button
{
public:
    CustomButton(const juce::String& buttonText): juce::Button(buttonText), displayText(buttonText) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);
        if (getToggleState()) {
            g.setColour(config::theme);
            g.drawRect(bounds, 2.0f);
        }
        else {
            g.setColour(config::buttonDisabled);
            g.drawRect(bounds, 1.0f);
        }
        g.setFont(13.0f);
        g.drawText(displayText, getLocalBounds(), juce::Justification::centred);
    }
private:
    juce::String displayText;
};

class CustomIconButton: public juce::Button
{
public:
    CustomIconButton(const char* svgData, int svgSize): juce::Button("IconButton") {
        if (svgData != nullptr) {
            drawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse(juce::String::fromUTF8(svgData, svgSize)));
        }
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);
        if (getToggleState()) {
            g.setColour(config::theme);
            g.drawRect(bounds, 2.0f);
        }
        else {
            g.setColour(config::buttonDisabled);
            g.drawRect(bounds, 1.0f);
        }
        if (drawable != nullptr) {
            drawable->replaceColour(juce::Colours::black, juce::Colours::white);
            drawable->drawWithin(g, bounds.reduced(6.0f), juce::RectanglePlacement::centred, 1.0f);
        }
    }
private:
    std::unique_ptr<juce::Drawable> drawable;
};

class FilterBandControl: public juce::Component
{
public:
    FilterBandControl(juce::AudioProcessorValueTreeState& apvts, int bandIndex) {
        filterModeComboBox.setJustificationType(juce::Justification::centred);
        filterModeComboBox.addItemList(config::filterModes, 1);
        channelModeComboBox.setJustificationType(juce::Justification::centred);
        channelModeComboBox.addItemList(config::channelModes, 1);
        bypassButton.setClickingTogglesState(true);
        for (auto& s : bandSliders) {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
            addAndMakeVisible(s);
        }
        for (auto* c : allComponents) {
            addAndMakeVisible(c);
        }
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, config::getID(config::ID_BAND_BYPASS, bandIndex), bypassButton);
        channelModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, config::getID(config::ID_BAND_CHANNEL, bandIndex), channelModeComboBox);
        filterModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, config::getID(config::ID_BAND_FILTER, bandIndex), filterModeComboBox);
        freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, config::getID(config::ID_BAND_FREQ, bandIndex), bandSliders[0]);
        gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, config::getID(config::ID_BAND_GAIN, bandIndex), bandSliders[1]);
        qualAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, config::getID(config::ID_BAND_QUAL, bandIndex), bandSliders[2]);
    };
    ~FilterBandControl() override {};
    void paintOverChildren(juce::Graphics& g) override {
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);

        for (int i = 0; i < 3; ++i) {
            juce::Slider& s = bandSliders[i];
            auto b = s.getBounds().toFloat();
            auto knobArea = b.withTrimmedBottom((float)s.getTextBoxHeight());
            g.drawText(bandUnits[i], knobArea.reduced(-2.0f, 4.0f), juce::Justification::bottomRight);
        }
    }
    void resized() override {
        auto bounds = getLocalBounds();
        auto topHeader = bounds.removeFromTop(30);
        bypassButton.setBounds(topHeader.reduced(margin));
        auto third = bounds.removeFromTop(30);
        channelModeComboBox.setBounds(third.reduced(margin));
        auto secHeader = bounds.removeFromTop(30);
        filterModeComboBox.setBounds(secHeader.reduced(margin));
        bounds.reduce(margin, margin);
        int controlHeight = bounds.getHeight() / 3;

        for (int i = 0; i < 3; ++i) {
            bandSliders[i].setBounds(bounds.removeFromTop(controlHeight).reduced(margin));
        }
    };
private:
    static constexpr int margin = 4;
    std::vector<juce::Component*> allComponents {&bypassButton, &channelModeComboBox, &filterModeComboBox};
    CustomButton bypassButton {config::ID_BAND_BYPASS};
    juce::ComboBox channelModeComboBox;
    juce::ComboBox filterModeComboBox;
    std::array<juce::Slider, 3> bandSliders;
    std::array<juce::String, 3> bandUnits {"Hz", "dB", "Q"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qualAttachment;
};

class CustomSlider: public juce::Slider { public:void mouseDoubleClick(const juce::MouseEvent& event) override {}; };
