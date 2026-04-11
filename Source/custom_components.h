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
            g.setColour(juce::Colour(zlth::ui::colors::theme));
            g.drawRect(bounds, 2.0f);
        }
        else {
            g.setColour(juce::Colour(zlth::ui::colors::buttonDisabled));
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
            g.setColour(juce::Colour(zlth::ui::colors::theme));
            g.drawRect(bounds, 2.0f);
        }
        else {
            g.setColour(juce::Colour(zlth::ui::colors::buttonDisabled));
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
        typeComboBox.setJustificationType(juce::Justification::centred);
        typeComboBox.addItemList(filterModes, 1);
        modeComboBox.setJustificationType(juce::Justification::centred);
        modeComboBox.addItemList(channelModes, 1);
        bypassButton.setClickingTogglesState(true);
        for (auto* s : {&freqSlider, &gainSlider, &qSlider}) {
            s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
        }
        for (auto* c : allComponents) {
            addAndMakeVisible(c);
        }
        const juce::String index = juce::String(bandIndex + 1);
        freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_BAND_FREQ + index, freqSlider);
        gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_BAND_GAIN + index, gainSlider);
        qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_BAND_QUAL + index, qSlider);
        typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ID_BAND_FILTER + index, typeComboBox);
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ID_BAND_BYPASS + index, bypassButton);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ID_BAND_CHANNEL + index, modeComboBox);
    };
    ~FilterBandControl() override {};
    void paintOverChildren(juce::Graphics& g) override {
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        auto drawUnit = [&](juce::Slider& s, const char* unit) {
            auto b = s.getBounds().toFloat();
            auto knobArea = b.withTrimmedBottom((float)s.getTextBoxHeight());
            g.drawText(unit, knobArea.reduced(-2.0f, 4.0f), juce::Justification::bottomRight);
        };

        drawUnit(freqSlider, "Hz");
        drawUnit(gainSlider, "dB");
        drawUnit(qSlider, "Q");
    }
    void resized() override {
        auto bounds = getLocalBounds();
        auto topHeader = bounds.removeFromTop(30);
        bypassButton.setBounds(topHeader.reduced(margin));
        auto third = bounds.removeFromTop(30);
        modeComboBox.setBounds(third.reduced(margin));
        auto secHeader = bounds.removeFromTop(30);
        typeComboBox.setBounds(secHeader.reduced(margin));
        bounds.reduce(margin, margin);
        int controlHeight = bounds.getHeight() / 3;
        freqSlider.setBounds(bounds.removeFromTop(controlHeight).reduced(margin));
        gainSlider.setBounds(bounds.removeFromTop(controlHeight).reduced(margin));
        qSlider.setBounds(bounds.reduced(margin));
    };
private:
    static constexpr int margin = 4;
    std::vector<juce::Component*> allComponents {&typeComboBox, &modeComboBox, &bypassButton, &freqSlider, &gainSlider, &qSlider};
    CustomButton bypassButton {ID_BAND_BYPASS};
    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::ComboBox typeComboBox;
    juce::ComboBox modeComboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
};

class CustomSlider: public juce::Slider { public:void mouseDoubleClick(const juce::MouseEvent& event) override {}; };
