#pragma once

#include <JuceHeader.h>
#include "zlth_fifo.h"
#include "zlth_dsp_filter.h"
#include "zlth_dsp_gain.h"
#include "config.h"

struct FilterSnapshot {
  zlth::dsp::Filter filter {};
  int channelMode = 0;
};

class QuasarEQAudioProcessor: public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener {
public:
  QuasarEQAudioProcessor();
  int getNumPrograms() override;
  int getCurrentProgram() override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const;
  bool hasEditor() const override;
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
  void releaseResources() override;
  void setCurrentProgram(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;
  void parameterChanged(const juce::String& parameterID, float) override;
  double getTailLengthSeconds() const override;
  const juce::String getName() const override;
  const juce::String getProgramName(int index);
  juce::AudioProcessorEditor* createEditor() override;
  juce::AudioProcessorValueTreeState apvts;
  juce::UndoManager undoManager;
  std::array<SampleFifo, 2> channelFifo {};
  void updateBands(uint32_t flags);
  static constexpr uint32_t PARAMS_MASK_BAND = (1u << config::BAND_COUNT) - 1;
private:
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;
  void update_global();
  std::atomic<uint32_t> updateFlags {PARAMS_MASK_BAND};
  std::atomic<bool> updateGlobalFlag {};
  std::array<std::array<zlth::dsp::Filter, config::BAND_COUNT>, 2> filters {};
  std::array<zlth::dsp::Gain, 2> gains {};
};
