#pragma once

#include "juce_PluginEditor.h"

QuasarEQAudioProcessorEditor::QuasarEQAudioProcessorEditor(QuasarEQAudioProcessor& p): AudioProcessorEditor(&p), audioProcessor(p), visualizerComponent(p) {
    setLookAndFeel(&customLNF);
    for (int i = 0; i < config::BAND_COUNT; ++i) {
        bandControls.push_back(std::make_unique<FilterBandControl>(audioProcessor.apvts, i));
        addAndMakeVisible(*bandControls.back());
    }
    for (int i = 0; i < config::iconCount; ++i) {
        auto btn = std::make_unique<CustomIconButton>(config::iconData[i], config::iconSize[i]);
        btn->setRadioGroupId(1001);
        btn->setClickingTogglesState(true);
        btn->onClick = [this, i] {
            if (paletteButtons[i]->getToggleState()) {
                selectedFilterType = i;
            }
        };
        addAndMakeVisible(*btn);
        paletteButtons.push_back(std::move(btn));
    }
    paletteButtons[selectedFilterType]->setToggleState(true, juce::dontSendNotification);

    for (int i = 0; i < config::modeNames.size(); ++i) {
        auto btn = std::make_unique<CustomButton>(config::modeNames[i]);
        btn->setRadioGroupId(1002);
        btn->setClickingTogglesState(true);

        btn->onClick = [this, i] {
            if (modeButtons[i]->getToggleState()) {
                selectedMode = i;
            }
        };
        addAndMakeVisible(*btn);
        modeButtons.push_back(std::move(btn));
    }
    modeButtons[selectedMode]->setToggleState(true, juce::dontSendNotification);

    visualizerComponent.getSelectedTypeCallback = [this] { return selectedFilterType; };
    visualizerComponent.getMSTypeCallback = [this] { return selectedMode; };

    addAndMakeVisible(visualizerComponent);

    for (int i = 0; i < masterGainSliders.size(); ++i) {
        auto& slider = masterGainSliders[i];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 16);
        addAndMakeVisible(slider);

        auto label = std::make_unique<juce::Label>("", config::masterGainLabels[i]);
        label->setJustificationType(juce::Justification::horizontallyCentred);
        label->setFont(12.0f);
        addAndMakeVisible(*label);
        masterGainLabelsComponents[i] = std::move(label);

        masterGainAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, getMasterGainIDs()[i], slider));
    }

    setSize(windowWidth, windowHeight);
}

QuasarEQAudioProcessorEditor::~QuasarEQAudioProcessorEditor() {
    setLookAndFeel(nullptr);
}

void QuasarEQAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(config::pluginBackground);
}

void QuasarEQAudioProcessorEditor::resized() {

    juce::Rectangle<int> mainArea = getLocalBounds().reduced(margin);
    int btnW = 45;

    juce::Rectangle<int> sectionA = mainArea.removeFromTop(sectionAHeight).reduced(margin);
    for (auto& btn : modeButtons) {
        if (btn) {
            btn->setBounds(sectionA.removeFromLeft(btnW * 2).reduced(1));
        }
    }

    juce::Rectangle<int> sectionB = mainArea.removeFromTop(sectionBHeight).reduced(margin);
    for (auto& btn : paletteButtons) {
        if (btn) {
            btn->setBounds(sectionB.removeFromLeft(btnW).reduced(1));
        }
    }

    juce::Rectangle<int> sectionC = mainArea.removeFromTop(sectionCHeight);
    visualizerComponent.setBounds(sectionC);

    juce::Rectangle<int> sectionD = mainArea.removeFromTop(sectionDHeight);

    auto sectionD1 = sectionD.removeFromRight(86);

    int knowbWidth = sectionD1.getWidth() / masterGainSliders.size();
    for (int i = 0; i < masterGainSliders.size(); ++i) {
        auto area = sectionD1.removeFromLeft(knowbWidth).reduced(margin);
        if (masterGainLabelsComponents[i]) {
            masterGainLabelsComponents[i]->setBounds(area.removeFromTop(12));
        }
        masterGainSliders[i].setBounds(area);
    }

    const int bandWidth = sectionD.getWidth() / config::BAND_COUNT;
    for (int i = 0; i < config::BAND_COUNT; ++i) {
        if (bandControls[i]) {
            bandControls[i]->setBounds(sectionD.removeFromLeft(bandWidth));
        }
    }
}
