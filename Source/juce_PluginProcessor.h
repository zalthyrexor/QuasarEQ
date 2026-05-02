#pragma once

#include <JuceHeader.h>
#include "zlth_fifo.h"
#include "zlth_dsp_filter.h"
#include "zlth_dsp_gain.h"
#include "config.h"

struct ParamListenerBridge: public juce::AudioProcessorValueTreeState::Listener {
  ParamListenerBridge(int index, std::atomic<uint64_t>& flags): paramIndex(index), updateFlags(flags) {
  }
  void parameterChanged(const juce::String&, float) override {
    updateFlags.fetch_or(1ull << paramIndex);
  }
  int paramIndex;
  std::atomic<uint64_t>& updateFlags;
};

struct FilterSnapshot {
  zlth::dsp::Filter filter {};
  int channelMode = 0;
};

class QuasarEQAudioProcessor: public juce::AudioProcessor {
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
  double getTailLengthSeconds() const override;
  const juce::String getName() const override;
  const juce::String getProgramName(int index);
  juce::AudioProcessorEditor* createEditor() override;
  juce::AudioProcessorValueTreeState apvts;
  juce::UndoManager undoManager;
  std::array<SampleFifo, 2> channelFifo {};
  void updateBands(uint64_t flags);
  void initializeAllParameters() const;
private:
  static constexpr uint64_t PARAMS_MASK_ALL {~0ull};
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;
  std::atomic<uint64_t> updateFlags {PARAMS_MASK_ALL};
  std::array<std::array<zlth::dsp::Filter, config::BAND_COUNT>, 2> filters {};
  std::array<std::array<std::array<zlth::dsp::Filter, config::BUTTER_COUNT>, config::BUTTER_MAX>, 2> butters {};
  std::array<zlth::dsp::Gain, 2> gains {};
  juce::OwnedArray<ParamListenerBridge> bridges;
};
