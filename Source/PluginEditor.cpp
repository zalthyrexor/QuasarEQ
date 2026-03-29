#pragma once

#include "PluginEditor.h"

QuasarEQAudioProcessorEditor::QuasarEQAudioProcessorEditor(QuasarEQAudioProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p), visualizerComponent(p)
{
    setLookAndFeel(&customLNF);
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        bandControls.push_back(std::make_unique<FilterBandControl>(audioProcessor.apvts, i));
        addAndMakeVisible(*bandControls.back());
    }
    for (int i = 0; i < icons.size(); ++i)
    {
        auto btn = std::make_unique<CustomIconButton>(icons[i].data, icons[i].size);
        btn->setRadioGroupId(1001);
        btn->setClickingTogglesState(true);
        btn->onClick = [this, i]
            {
                if (paletteButtons[i]->getToggleState())
                    selectedFilterType = i;
            };
        addAndMakeVisible(*btn);
        paletteButtons.push_back(std::move(btn));
    }
    paletteButtons[selectedFilterType]->setToggleState(true, juce::dontSendNotification);

    visualizerComponent.getSelectedTypeCallback = [this] { return selectedFilterType; };

    pluginInfoLabel.setText(juce::String("Zalthyrexor - " + juce::String(JucePlugin_Name)).toUpperCase(), juce::dontSendNotification);
    pluginInfoLabel.setJustificationType(juce::Justification::horizontallyCentred);
    pluginInfoLabel.setFont(16.0f);
    gainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
    addAndMakeVisible(visualizerComponent);
    addAndMakeVisible(pluginInfoLabel);
    addAndMakeVisible(gainSlider);
    outGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, ID_GAIN, gainSlider);
    setSize(windowWidth, windowHeight);
}
QuasarEQAudioProcessorEditor::~QuasarEQAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}
void QuasarEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(zlth::ui::colors::pluginBackground));
}
void QuasarEQAudioProcessorEditor::resized()
{
    juce::Rectangle<int> mainArea = getLocalBounds().reduced(margin);
    juce::Rectangle<int> sectionA = mainArea.removeFromTop(sectionAHeight).reduced(margin);
    juce::Rectangle<int> sectionB = mainArea.removeFromTop(sectionBHeight).reduced(margin);
    juce::Rectangle<int> sectionC = mainArea.removeFromTop(sectionCHeight).reduced(margin);
    juce::Rectangle<int> sectionD = mainArea.removeFromTop(sectionDHeight).reduced(margin);
    pluginInfoLabel.setBounds(sectionA.reduced(margin));
    sectionB.reduce(margin, margin);
    sectionB.removeFromRight(60);
    auto amount = sectionB.getWidth() * 0.19;
    sectionB.removeFromRight(amount);
    sectionB.removeFromLeft(amount);
    const int numButtons = static_cast<int>(paletteButtons.size());
    int btnW = sectionB.getWidth() / numButtons;
    for (auto& btn : paletteButtons)
    {
        if (btn)
        {
            btn->setBounds(sectionB.removeFromLeft(btnW).reduced(1));
        }
    }
    visualizerComponent.setBounds(sectionC);
    gainSlider.setBounds(sectionD.removeFromRight(20 * 3).reduced(margin));
    sectionD.reduce(margin, margin);
    const int bandWidth = sectionD.getWidth() / NUM_BANDS;
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        if (bandControls[i])
        {
            bandControls[i]->setBounds(sectionD.removeFromLeft(bandWidth));
        }
    }
}
