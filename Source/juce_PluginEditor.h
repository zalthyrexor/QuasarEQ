#pragma once

#include "VisualizerComponent.h"
#include "juce_PluginEditor.h"

class CustomButton: public juce::Button {
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

class CustomIconButton: public juce::Button {
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

class FilterBandControl: public juce::Component {
public:
  FilterBandControl(juce::AudioProcessorValueTreeState& apvts, int bandIndex) {
    bypassButton.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, config::toID(config::ID_BAND_BYPASS, bandIndex), bypassButton);
    addAndMakeVisible(bypassButton);
    for (int i = 0; i < comboBoxCount; ++i) {
      auto& b = comboBoxes[i];
      b.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(b);
      b.addItemList(comboArrays[i], 1);
      comboBoxAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, config::toID(comboBoxIDs[i], bandIndex), b);
    }
    for (int i = 0; i < sliderCount; ++i) {
      auto& s = bandSliders[i];
      s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 16);
      addAndMakeVisible(s);
      bandAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, config::toID(bandIDs[i], bandIndex), s);
    }
  }
  ~FilterBandControl() override {
  };
  void paintOverChildren(juce::Graphics& g) override {
    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    for (int i = 0; i < sliderCount; ++i) {
      juce::Slider& s = bandSliders[i];
      auto b = s.getBounds().toFloat();
      auto knobArea = b.withTrimmedBottom((float)s.getTextBoxHeight());
      g.drawText(config::bandUnits[i], knobArea.reduced(-2.0f, 4.0f), juce::Justification::bottomRight);
    }
  }
  void resized() override {
    auto bounds = getLocalBounds();
    bypassButton.setBounds(bounds.removeFromTop(28).reduced(margin));
    for (int i = 0; i < comboBoxCount; ++i) {
      comboBoxes[i].setBounds(bounds.removeFromTop(28).reduced(margin));
    }
    int controlHeight = bounds.getHeight() / sliderCount;
    for (int i = 0; i < sliderCount; ++i) {
      bandSliders[i].setBounds(bounds.removeFromTop(controlHeight).reduced(margin));
    }
  }
private:
  static constexpr int margin = 2;
  CustomButton bypassButton {config::ID_BAND_BYPASS};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
  static constexpr int comboBoxCount {2};
  std::array<juce::ComboBox, comboBoxCount> comboBoxes;
  std::array<juce::String, comboBoxCount> comboBoxIDs {config::ID_BAND_CHANNEL, config::ID_BAND_FILTER};
  std::array<juce::StringArray, comboBoxCount> comboArrays {config::channelModes, config::filterModes};
  std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, comboBoxCount> comboBoxAttachments;
  static constexpr int sliderCount {3};
  std::array<juce::Slider, sliderCount> bandSliders;
  std::array<juce::String, sliderCount> bandIDs {config::ID_BAND_GAIN, config::ID_BAND_FREQ, config::ID_BAND_QUAL};
  std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, sliderCount> bandAttachments;
};

class CustomSlider: public juce::Slider {
public:
  void mouseDoubleClick(const juce::MouseEvent&) override {
  };
};

class QuasarEQAudioProcessorEditor: public juce::AudioProcessorEditor {
public:

  QuasarEQAudioProcessorEditor(QuasarEQAudioProcessor& p): AudioProcessorEditor(&p), audioProcessor(p), visualizerComponent(p) {
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

  ~QuasarEQAudioProcessorEditor() {
    setLookAndFeel(nullptr);
  }

  void paint(juce::Graphics& g) override {
    g.fillAll(config::pluginBackground);
  }

  void resized() override {
    juce::Rectangle<int> mainArea = getLocalBounds().reduced(margin);
    juce::Rectangle<int> sectionA = mainArea.removeFromTop(sectionAHeight).reduced(margin);
    juce::Rectangle<int> sectionB = mainArea.removeFromTop(sectionBHeight).reduced(margin);
    juce::Rectangle<int> sectionC = mainArea.removeFromTop(sectionCHeight);
    juce::Rectangle<int> sectionD = mainArea.removeFromTop(sectionDHeight);
    juce::Rectangle<int> sectionD2 = sectionD.removeFromRight(96);
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
      masterGainSliderLabels[i]->setBounds(area.removeFromTop(18).reduced(margin));
      masterGainSliders[i]->setBounds(area.reduced(margin));
    }
    const int bandWidth = sectionD.getWidth() / config::BAND_COUNT;
    for (int i = 0; i < config::BAND_COUNT; ++i) {
      auto area = sectionD.removeFromLeft(bandWidth);
      bandControlLabels[i]->setBounds(area.removeFromTop(18).reduced(margin));
      bandControls[i]->setBounds(area);
    }
  }

private:
  static constexpr int margin = 2;
  static constexpr int sectionAHeight = 28;
  static constexpr int sectionBHeight = 28;
  static constexpr int sectionCHeight = 300;
  static constexpr int sectionDHeight = 340;
  static constexpr int windowWidth = 684;
  static constexpr int windowHeight = margin * 2 + sectionAHeight + sectionBHeight + sectionCHeight + sectionDHeight;

  static constexpr int channelBtnW = 90;
  static constexpr int filterBtnW = 45;

  int selectedChannnel {config::PARAM_CHANNEL_DEFAULT};
  int selectedFilter {config::PARAM_FILTER_DEFAULT};

  CustomLNF customLNF;
  QuasarEQAudioProcessor& audioProcessor;
  VisualizerComponent visualizerComponent;

  std::vector<std::unique_ptr<CustomSlider>> masterGainSliders;
  std::vector<std::unique_ptr<juce::Label>> masterGainSliderLabels;
  std::vector<std::unique_ptr<CustomButton>> channelModeButtons;
  std::vector<std::unique_ptr<CustomIconButton>> filterModeButtons;
  std::vector<std::unique_ptr<FilterBandControl>> bandControls;
  std::vector<std::unique_ptr<juce::Label>> bandControlLabels;
  std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> masterGainAttachments;

  static auto getMasterGainIDs() -> const std::array<juce::String, 2>& {
    static const std::array<juce::String, 2> ids
    {
      config::ID_OUT_GAIN_0, config::ID_OUT_GAIN_1
    };
    return ids;
  }
};
