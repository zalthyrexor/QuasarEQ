#pragma once

#include "VisualizerComponent.h"

class CustomButton: public juce::Button
{
public:
    CustomButton() : juce::Button("PowerButton") {}
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        const auto isBypass = getToggleState();
        const auto color = isBypass ? juce::Colour(zlth::ui::colors::theme) : juce::Colour(zlth::ui::colors::buttonDisabled);
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);
        g.setColour(color);
        g.drawRect(bounds, 2.0f);
        g.setFont(13.0f);
        g.drawText("Bypass", getLocalBounds(), juce::Justification::centred);
    }
    void mouseEnter(const juce::MouseEvent& event) override
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void mouseExit(const juce::MouseEvent& event) override
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
};

class FilterBandControl: public juce::Component
{
public:
    FilterBandControl(juce::AudioProcessorValueTreeState& apvts, int bandIndex)
    {
        typeComboBox.setJustificationType(juce::Justification::centred);
        typeComboBox.addItemList(filterTags, 1);
        bypassButton.setClickingTogglesState(true);
        for (auto* s : {&freqSlider, &gainSlider, &qSlider})
        {
            s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
        }
        for (auto* c : allComponents)
        {
            addAndMakeVisible(c);
        }
        const juce::String index = juce::String(bandIndex + 1);
        freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_PREFIX_FREQ + index, freqSlider);
        gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_PREFIX_GAIN + index, gainSlider);
        qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_PREFIX_QUAL + index, qSlider);
        typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ID_PREFIX_TYPE + index, typeComboBox);
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ID_PREFIX_BYPASS + index, bypassButton);
    };
    ~FilterBandControl() override {};
    void resized() override
    {
        auto bounds = getLocalBounds();
        auto topHeader = bounds.removeFromTop(30);
        bypassButton.setBounds(topHeader.reduced(margin));
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
    std::vector<juce::Component*> allComponents {&typeComboBox, &bypassButton, &freqSlider, &gainSlider, &qSlider};
    CustomButton bypassButton;
    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::ComboBox typeComboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
};

class CustomSlider: public juce::Slider { public:void mouseDoubleClick(const juce::MouseEvent& event) override {}; };

class CustomIconButton: public juce::Button
{
public:
    CustomIconButton(const char* svgData, int svgSize) : juce::Button("IconButton")
    {
        if (svgData != nullptr)
        {
            drawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse(juce::String::fromUTF8(svgData, svgSize)));
        }
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);
        if (getToggleState())
        {
            g.setColour(juce::Colour(zlth::ui::colors::theme));
            g.drawRect(bounds, 2.0f);
        }
        else {

            g.setColour(juce::Colour(zlth::ui::colors::buttonDisabled));
            g.drawRect(bounds, 1.0f);
        }
        if (drawable != nullptr)
        {
            drawable->replaceColour(juce::Colours::black, juce::Colours::white);
            drawable->drawWithin(g, bounds.reduced(6.0f), juce::RectanglePlacement::centred, 1.0f);
        }
    }
private:
    std::unique_ptr<juce::Drawable> drawable;
};