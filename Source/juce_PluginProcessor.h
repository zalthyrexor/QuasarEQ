#pragma once

#include <JuceHeader.h>
#include "zlth_fifo.h"
#include "zlth_dsp_filter.h"
#include "config.h"
#include "EQProcessor.h"

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
  void coefsSetter(zlth::dsp::Filter& filter, int i, float sampleRate);
  juce::AudioProcessorEditor* createEditor() override;
  juce::AudioProcessorValueTreeState apvts;
  juce::UndoManager undoManager;
  std::array<SampleFifo, 2> channelFifo {};
  std::vector<FilterSnapshot> getFilterSnapshots();
private:
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;
  void updateBands(uint32_t flags);
  static constexpr uint32_t PARAMS_MASK_BAND = (1u << config::BAND_COUNT) - 1;
  static constexpr uint32_t PARAMS_MASK_OUT = (1u << config::BAND_COUNT + 1) - 1;
  static constexpr uint32_t PARAMS_MASK_ALL = PARAMS_MASK_BAND | PARAMS_MASK_OUT;
  std::atomic<uint32_t> updateFlags {PARAMS_MASK_ALL};
  std::array<ProcessChain<config::BAND_COUNT>, 2> processors {};
};
