#pragma once

#include "juce_PluginEditor.h"

QuasarEQAudioProcessorEditor::QuasarEQAudioProcessorEditor(QuasarEQAudioProcessor& p): AudioProcessorEditor(&p), audioProcessor(p), visualizerComponent(p) {
  setLookAndFeel(&customLNF);
 
  for (int i = 0; i < config::iconCount; ++i) {
    auto btn = std::make_unique<CustomIconButton>(config::iconData[i], config::iconSize[i]);
    btn->setRadioGroupId(1001);
    btn->setClickingTogglesState(true);
    btn->onClick = [this, i] {
      if (filterModeButtons[i]->getToggleState()) {
        selectedFilter = i;
      }
    };
    addAndMakeVisible(*btn);
    filterModeButtons.push_back(std::move(btn));
  }
  filterModeButtons[selectedFilter]->setToggleState(true, juce::dontSendNotification);
  visualizerComponent.getFilterModeCallback = [this] { return selectedFilter; };

  for (int i = 0; i < config::modeNames.size(); ++i) {
    auto btn = std::make_unique<CustomButton>(config::modeNames[i]);
    btn->setRadioGroupId(1002);
    btn->setClickingTogglesState(true);
    btn->onClick = [this, i] {
      if (channelModeButtons[i]->getToggleState()) {
        selectedChannnel = i;
      }
    };
    addAndMakeVisible(*btn);
    channelModeButtons.push_back(std::move(btn));
  }
  channelModeButtons[selectedChannnel]->setToggleState(true, juce::dontSendNotification);
  visualizerComponent.getChannelModeCallback = [this] { return selectedChannnel; };

  addAndMakeVisible(visualizerComponent);

  for (int i = 0; i < config::BAND_COUNT; ++i) {
    bandControls.push_back(std::make_unique<FilterBandControl>(audioProcessor.apvts, i));
    addAndMakeVisible(*bandControls.back());

    auto text = config::toID(juce::String("BAND"), i);
    auto label = std::make_unique<juce::Label>("", text);
    label->setJustificationType(juce::Justification::horizontallyCentred);
    label->setFont(12.0f);
    addAndMakeVisible(*label);
    bandControlLabels.push_back(std::move(label));
  }

  for (int i = 0; i < getMasterGainIDs().size(); ++i) {
    auto slider = std::make_unique<CustomSlider>();
    slider->setSliderStyle(juce::Slider::LinearVertical);
    slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 16);
    addAndMakeVisible(*slider);
    masterGainAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, getMasterGainIDs()[i], *slider));
    masterGainSliders.push_back(std::move(slider));

    auto label = std::make_unique<juce::Label>("", config::masterGainLabels[i]);
    label->setJustificationType(juce::Justification::horizontallyCentred);
    label->setFont(12.0f);
    addAndMakeVisible(*label);
    masterGainSliderLabels.push_back(std::move(label));
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

  juce::Rectangle<int> sectionA = mainArea.removeFromTop(sectionAHeight).reduced(margin);
  juce::Rectangle<int> sectionB = mainArea.removeFromTop(sectionBHeight).reduced(margin);
  juce::Rectangle<int> sectionC = mainArea.removeFromTop(sectionCHeight);
  juce::Rectangle<int> sectionD = mainArea.removeFromTop(sectionDHeight);
  juce::Rectangle<int> sectionD2 = sectionD.removeFromRight(98);

  for (auto& btn : channelModeButtons) {
    btn->setBounds(sectionA.removeFromLeft(channelBtnW).reduced(1));
  }

  for (auto& btn : filterModeButtons) {
    btn->setBounds(sectionB.removeFromLeft(filterBtnW).reduced(1));
  }

  visualizerComponent.setBounds(sectionC);


  const int masterGainWidth = sectionD2.getWidth() / masterGainSliders.size();
  for (int i = 0; i < masterGainSliders.size(); ++i) {
    auto area = sectionD2.removeFromLeft(masterGainWidth);
    masterGainSliderLabels[i]->setBounds(area.removeFromTop(20).reduced(margin));
    masterGainSliders[i]->setBounds(area.reduced(margin));
  }

  const int bandWidth = sectionD.getWidth() / config::BAND_COUNT;
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    auto area = sectionD.removeFromLeft(bandWidth);
    bandControlLabels[i]->setBounds(area.removeFromTop(20).reduced(margin));
    bandControls[i]->setBounds(area);
  }
}
