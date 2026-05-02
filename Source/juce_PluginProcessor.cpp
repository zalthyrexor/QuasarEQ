#include "juce_PluginProcessor.h"
#include "juce_PluginEditor.h"
#include "unit.h"

QuasarEQAudioProcessor::QuasarEQAudioProcessor():
  AudioProcessor(BusesProperties().
    withInput("Input", juce::AudioChannelSet::stereo(), true).
    withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, &undoManager, config::ID_PARAMETERS, createParameterLayout()) {
  int bitIndex = 0;
  for (int i = 0; i < config::ID_OUT_GAIN.size(); ++i) {
    auto* bridge = new ParamListenerBridge(bitIndex, updateFlags);
    bridges.add(bridge);
    apvts.addParameterListener(config::ID_OUT_GAIN[i], bridge);
    ++bitIndex;
  }
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    for (const auto& prefix : config::bandParamPrefixes) {
      auto* bridge = new ParamListenerBridge(bitIndex, updateFlags);
      bridges.add(bridge);
      apvts.addParameterListener(config::toID(prefix, i), bridge);
    }
    ++bitIndex;
  }
  for (int i = 0; i < config::BUTTER_COUNT; ++i) {
    for (const auto& prefix : config::butterworthPrefixes) {
      auto* bridge = new ParamListenerBridge(bitIndex, updateFlags);
      bridges.add(bridge);
      apvts.addParameterListener(config::IndexToButterworthID(prefix, i), bridge);
    }
    ++bitIndex;
  }
}

juce::AudioProcessorValueTreeState::ParameterLayout QuasarEQAudioProcessor::createParameterLayout() const {
  auto logFrom0To1 = [](float start, float end, float proportion) {
    return start * std::pow(end / start, proportion);
  };
  auto logTo0To1 = [](float start, float end, float v) {
    return std::log(v / start) / std::log(end / start);
  };
  auto makeAttrs = [](int decimals, const juce::String& label) {
    return juce::AudioParameterFloatAttributes()
      .withStringFromValueFunction([decimals](float v, int) { return juce::String(v, decimals); })
      .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); })
      .withLabel(label);
  };
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  juce::NormalisableRange<float> gainRange {config::PARAM_GAIN_MIN, config::PARAM_GAIN_MAX};
  juce::NormalisableRange<float> freqRange {config::PARAM_FREQ_MIN, config::PARAM_FREQ_MAX, logFrom0To1, logTo0To1};
  juce::NormalisableRange<float> qualRange {config::PARAM_QUAL_MIN, config::PARAM_QUAL_MAX, logFrom0To1, logTo0To1};
  auto gainAttrs = makeAttrs(2, config::bandUnits[0]);
  auto freqAttrs = makeAttrs(2, config::bandUnits[1]);
  auto qualAttrs = makeAttrs(2, config::bandUnits[2]);
  for (int i = 0; i < config::ID_OUT_GAIN.size(); ++i) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(config::ID_OUT_GAIN[i], config::ID_OUT_GAIN[i], gainRange, config::PARAM_GAIN_DEF, gainAttrs));
  }
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    const auto id = [i](auto prefix) { return config::toID(prefix, i); };
    float proportion = static_cast<float>(i + 1) / static_cast<float>(config::BAND_COUNT + 1);
    float initialFreq = config::PARAM_FREQ_MIN * std::pow(config::PARAM_FREQ_MAX / config::PARAM_FREQ_MIN, proportion);
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_GAIN), id(config::ID_BAND_GAIN), gainRange, config::PARAM_GAIN_DEF, gainAttrs));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_FREQ), id(config::ID_BAND_FREQ), freqRange, initialFreq, freqAttrs));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_QUAL), id(config::ID_BAND_QUAL), qualRange, config::PARAM_QUAL_DEF, qualAttrs));
    layout.add(std::make_unique<juce::AudioParameterChoice>(id(config::ID_BAND_FILTER), id(config::ID_BAND_FILTER), config::filterModes, config::PARAM_FILTER_DEFAULT));
    layout.add(std::make_unique<juce::AudioParameterChoice>(id(config::ID_BAND_CHANNEL), id(config::ID_BAND_CHANNEL), config::channelModes, config::PARAM_CHANNEL_DEFAULT));
    layout.add(std::make_unique<juce::AudioParameterBool>(id(config::ID_BAND_BYPASS), id(config::ID_BAND_BYPASS), config::PARAM_BYPASS_DEFAULT));
  }
  for (int i = 0; i < config::BUTTER_COUNT; ++i) {
    const auto id = [i](auto prefix) { return config::IndexToButterworthID(prefix, i); };
    float initialFreq = 500.0f;
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_FREQ), id(config::ID_BAND_FREQ), freqRange, initialFreq, freqAttrs));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_ORDER), id(config::ID_BAND_ORDER), freqRange, initialFreq, freqAttrs));
  }
  return layout;
}

void QuasarEQAudioProcessor::initializeAllParameters() const {
  auto reset = [&](auto id) {
    if (auto* p = apvts.getParameter(id)) {
      p->setValueNotifyingHost(p->getDefaultValue());
    }
  };
  for (int i = 0; i < config::ID_OUT_GAIN.size(); ++i) {
    reset(config::ID_OUT_GAIN[i]);
  }
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    for (const auto& prefix : config::bandParamPrefixes) {
      reset(config::toID(prefix, i));
    }
  }
}

void QuasarEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  channelFifo[0].prepare(samplesPerBlock);
  channelFifo[1].prepare(samplesPerBlock);
  updateBands(PARAMS_MASK_ALL);
}

void QuasarEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
    buffer.clear(i, 0, buffer.getNumSamples());
  }
  if (auto flags = updateFlags.exchange(0)) {
    updateBands(flags);
  }
  const auto numSamples = static_cast<size_t>(buffer.getNumSamples());
  std::span<float> span[] {{buffer.getWritePointer(0), numSamples}, {buffer.getWritePointer(1), numSamples}};
  zlth::simd::hadamard_butterfly(span[0], span[1]);
  for(int c = 0; c < 2; ++c){
    for (int i = 0; i < config::BAND_COUNT; ++i) {
      filters[c][i].process(span[c]);
    }
    for (int i = 0; i < config::BUTTER_COUNT; ++i) {
      for (int j = 0; j < config::BUTTER_MAX; ++j) {
        butters[c][j][i].process(span[c]);
      }
    }
    gains[c].process(span[c]);
    channelFifo[c].update(span[c]);
  }
  zlth::simd::hadamard_butterfly(span[0], span[1]);
}

void QuasarEQAudioProcessor::updateBands(uint64_t flags) {
  const float sr = getSampleRate();
  int bitIndex = 0;
  for (int i = 0; i < config::ID_OUT_GAIN.size(); ++i) {
    if (flags & (1ull << bitIndex)) {
      gains[i].set_gain(zlth::unit::dbToMag(apvts.getRawParameterValue(config::ID_OUT_GAIN[i])->load()) * 0.5f);
    }
    ++bitIndex;
  }
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    if (flags & (1ull << bitIndex)) {
      auto load = [this, i](const juce::String& prefix) {
        return apvts.getRawParameterValue(config::toID(prefix, i))->load();
      };
      auto p0 = std::tan(std::numbers::pi_v<float> *std::min(load(config::ID_BAND_FREQ) / sr, 0.4999f));
      auto p1 = 1.0f / std::max(load(config::ID_BAND_QUAL), 0.0001f);
      auto p2 = zlth::unit::dbToMagFourthRoot(load(config::ID_BAND_GAIN));
      filters[0][i].set_coefficients(p0, p1, p2);
      filters[1][i].set_coefficients(p0, p1, p2);
      bool isActive = load(config::ID_BAND_BYPASS) < 0.5f;
      auto mode = static_cast<int>(load(config::ID_BAND_CHANNEL));
      auto type = static_cast<zlth::dsp::Filter::FilterType>((int)load(config::ID_BAND_FILTER));
      auto b0 = (isActive && (mode == 0 || mode == 1)) ? type : zlth::dsp::Filter::FilterType::PassThrough;
      auto b1 = (isActive && (mode == 0 || mode == 2)) ? type : zlth::dsp::Filter::FilterType::PassThrough;
      filters[0][i].set_filter_type(b0);
      filters[1][i].set_filter_type(b1);
    }
    ++bitIndex;
  }
  for (int i = 0; i < config::BUTTER_COUNT; ++i) {
    if (flags & (1ull << bitIndex)) {
      auto load = [this, i](const juce::String& prefix) {
        return apvts.getRawParameterValue(config::IndexToButterworthID(prefix, i))->load();
      };
      auto p0 = std::tan(std::numbers::pi_v<float> *std::min(load(config::ID_BAND_FREQ) / sr, 0.4999f));
      auto p1 = 1.414;
      auto p2 = 1.0f;
      for (int j = 0; j < config::BUTTER_MAX; ++j) {
        butters[0][j][i].set_coefficients(p0, p1, p2);
        butters[1][j][i].set_coefficients(p0, p1, p2);
        butters[0][j][i].set_filter_type(zlth::dsp::Filter::FilterType::LowPass);
        butters[1][j][i].set_filter_type(zlth::dsp::Filter::FilterType::LowPass);
      }
    }
    ++bitIndex;
  }
}

void QuasarEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  juce::MemoryOutputStream stream(destData, false);
  apvts.state.writeToStream(stream);
}

void QuasarEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
  if (tree.isValid()) {
    apvts.replaceState(tree);
  }
}

bool QuasarEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto mainInput = layouts.getMainInputChannelSet();
  const auto mainOutput = layouts.getMainOutputChannelSet();
  return mainInput == mainOutput && mainInput == juce::AudioChannelSet::stereo();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new QuasarEQAudioProcessor();
}
juce::AudioProcessorEditor* QuasarEQAudioProcessor::createEditor() {
  return new QuasarEQAudioProcessorEditor(*this);
}
int QuasarEQAudioProcessor::getNumPrograms() {
  return 1;
}
int QuasarEQAudioProcessor::getCurrentProgram() {
  return 0;
}
bool QuasarEQAudioProcessor::hasEditor() const {
  return true;
}
bool QuasarEQAudioProcessor::acceptsMidi() const {
  return false;
}
bool QuasarEQAudioProcessor::producesMidi() const {
  return false;
}
bool QuasarEQAudioProcessor::isMidiEffect() const {
  return false;
}
double QuasarEQAudioProcessor::getTailLengthSeconds() const {
  return 0.0;
}
const juce::String QuasarEQAudioProcessor::getName() const {
  return JucePlugin_Name;
}
const juce::String QuasarEQAudioProcessor::getProgramName(int index) {
  return {};
}
void QuasarEQAudioProcessor::releaseResources() {
}
void QuasarEQAudioProcessor::setCurrentProgram(int index) {
}
void QuasarEQAudioProcessor::changeProgramName(int index, const juce::String& newName) {
}
