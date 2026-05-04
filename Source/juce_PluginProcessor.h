#pragma once

#include <JuceHeader.h>
#include "zlth_fifo.h"
#include "zlth_dsp_filter.h"
#include "zlth_dsp_gain.h"
#include "config.h"

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
  void releaseResources() override {};
  void setCurrentProgram(int index) override {};
  void changeProgramName(int index, const juce::String& newName) override {};
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;
  double getTailLengthSeconds() const override;
  const juce::String getName() const override;
  const juce::String getProgramName(int index);
  juce::AudioProcessorEditor* createEditor() override;
  juce::AudioProcessorValueTreeState apvts;
  juce::UndoManager undoManager;
  std::array<SampleFifo, config::CHANNEL_COUNT> channelFifo {};
  void initializeAllParameters() const;
  void resetParam(const juce::String&) const;


  std::array<const std::atomic<float>*, config::CHANNEL_COUNT> globalGainTable;
  std::array<std::array<const std::atomic<float>*, config::butterPrefixCount>, config::BUTTER_COUNT> butterTable;
  std::array<std::array<const std::atomic<float>*, config::biquadPrefixCount>, config::BIQUAD_COUNT> biquadTable;

private:
  void updateBands();
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;

  std::array<zlth::dsp::Gain, config::CHANNEL_COUNT> gains {};
  std::array<std::array<zlth::dsp::Filter, config::CHANNEL_COUNT>, config::BIQUAD_COUNT> biquads {};
  std::array<std::array<std::array<zlth::dsp::Filter, config::CHANNEL_COUNT>, config::PARAM_ORDER_MAX>, config::BUTTER_COUNT> butters {};
};
