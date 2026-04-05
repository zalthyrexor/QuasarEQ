#pragma once

#include "juce_PluginEditor.h"

QuasarEQAudioProcessorEditor::QuasarEQAudioProcessorEditor(QuasarEQAudioProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p), visualizerComponent(p)
{
    setLookAndFeel(&customLNF);
    for (int i = 0; i < config::BAND_COUNT; ++i)
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
            {
                selectedFilterType = i;
            }
        };
        addAndMakeVisible(*btn);
        paletteButtons.push_back(std::move(btn));
    }
    paletteButtons[selectedFilterType]->setToggleState(true, juce::dontSendNotification);

    for (int i = 0; i < modeNames.size(); ++i)
    {
        auto btn = std::make_unique<CustomButton>(modeNames[i]);
        btn->setRadioGroupId(1002);
        btn->setClickingTogglesState(true);

        btn->onClick = [this, i]
        {
            if (modeButtons[i]->getToggleState())
            {
                selectedMode = i;
            }
        };
        addAndMakeVisible(*btn);
        modeButtons.push_back(std::move(btn));
    }
    modeButtons[selectedMode]->setToggleState(true, juce::dontSendNotification);

    visualizerComponent.getSelectedTypeCallback = [this] { return selectedFilterType; };
    visualizerComponent.getMSTypeCallback = [this] { return selectedMode; };

    pluginInfoLabel.setText(juce::String("Zalthyrexor - " + juce::String(JucePlugin_Name)).toUpperCase(), juce::dontSendNotification);
    pluginInfoLabel.setJustificationType(juce::Justification::horizontallyCentred);
    pluginInfoLabel.setFont(16.0f);
    addAndMakeVisible(visualizerComponent);
    addAndMakeVisible(pluginInfoLabel);

    for (int i = 0; i < masterGainSliders.size(); ++i)
    {
        auto& slider = masterGainSliders[i];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        addAndMakeVisible(slider);

        auto label = std::make_unique<juce::Label>("", masterGainLabels[i]);
        label->setJustificationType(juce::Justification::horizontallyCentred);
        label->setFont(12.0f);
        addAndMakeVisible(*label);
        masterGainLabelsComponents[i] = std::move(label);

        masterGainAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, getMasterGainIDs()[i], slider));
    }

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
    juce::Rectangle<int> sectionB = mainArea.removeFromTop(sectionBHeight).reduced(margin);
    juce::Rectangle<int> sectionC = mainArea.removeFromTop(sectionCHeight).reduced(margin);
    juce::Rectangle<int> sectionD = mainArea.removeFromTop(sectionDHeight).reduced(margin);

    sectionB.reduce(margin, margin);
    sectionB.removeFromRight(60);
    sectionB.removeFromLeft(14 + 20);
    sectionB.removeFromRight(14 + 20);

    int btnW = 44;

    for (auto& btn : paletteButtons)
    {
        if (btn) btn->setBounds(sectionB.removeFromLeft(btnW).reduced(1));
    }

    for (auto it = modeButtons.rbegin(); it != modeButtons.rend(); ++it)
    {
        auto& btn = *it;
        if (btn)
        {
            btn->setBounds(sectionB.removeFromRight(btnW).reduced(1));
        }
    }

    visualizerComponent.setBounds(sectionC);

    auto masterSectionArea = sectionD.removeFromRight(60).reduced(margin);
    int knowbWidth = masterSectionArea.getWidth() / masterGainSliders.size();
    for (int i = 0; i < masterGainSliders.size(); ++i)
    {
        auto area = masterSectionArea.removeFromLeft(knowbWidth);
        if (masterGainLabelsComponents[i])
            masterGainLabelsComponents[i]->setBounds(area.removeFromTop(12));
        masterGainSliders[i].setBounds(area);
    }

    const int bandWidth = sectionD.getWidth() / config::BAND_COUNT;
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        if (bandControls[i])
        {
            bandControls[i]->setBounds(sectionD.removeFromLeft(bandWidth));
        }
    }
}
