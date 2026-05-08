#include "juce_PluginProcessor.h"
#include "juce_PluginEditor.h"
#include "unit.h"

QuasarEQAudioProcessor::QuasarEQAudioProcessor():
  AudioProcessor(BusesProperties().
    withInput("Input", juce::AudioChannelSet::stereo(), true).
    withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, &undoManager, config::ID_PARAMETERS, createParameterLayout()) {
  apvts.addParameterListener(config::ID_OUT_GAIN_0, this);
  apvts.addParameterListener(config::ID_OUT_GAIN_1, this);
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    for (const auto& prefix : config::bandParamPrefixes) {
      apvts.addParameterListener(config::toID(prefix, i), this);
    }
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
  layout.add(std::make_unique<juce::AudioParameterFloat>(config::ID_OUT_GAIN_0, config::ID_OUT_GAIN_0, gainRange, config::PARAM_GAIN_DEF, gainAttrs));
  layout.add(std::make_unique<juce::AudioParameterFloat>(config::ID_OUT_GAIN_1, config::ID_OUT_GAIN_1, gainRange, config::PARAM_GAIN_DEF, gainAttrs));
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
  return layout;
}

void QuasarEQAudioProcessor::initializeAllParameters() const {
  auto reset = [&](auto id) {
    if (auto* p = apvts.getParameter(id)) {
      p->setValueNotifyingHost(p->getDefaultValue());
    }
  };
  reset(config::ID_OUT_GAIN_0);
  reset(config::ID_OUT_GAIN_1);
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    for (const auto& prefix : config::bandParamPrefixes) {
      reset(config::toID(prefix, i));
    }
  }
}

void QuasarEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  channelFifo[0].prepare(samplesPerBlock);
  channelFifo[1].prepare(samplesPerBlock);
  updateBands(PARAMS_MASK_BAND);
  update_global();
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
  if(updateGlobalFlag.exchange(false)){
    update_global();
  }
  const auto numSamples = static_cast<size_t>(buffer.getNumSamples());
  std::span<float> span0 {buffer.getWritePointer(0), numSamples};
  std::span<float> span1 {buffer.getWritePointer(1), numSamples};
  zlth::simd::hadamard_butterfly(span0, span1);
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    filters[0][i].process(span0);
    filters[1][i].process(span1);
  }
  gains[0].process(span0);
  gains[1].process(span1);
  channelFifo[0].update(span0);
  channelFifo[1].update(span1);
  zlth::simd::hadamard_butterfly(span0, span1);
}

void QuasarEQAudioProcessor::parameterChanged(const juce::String& parameterID, float) {
  if (parameterID == config::ID_OUT_GAIN_0 || parameterID == config::ID_OUT_GAIN_1) {
    updateGlobalFlag.store(true);
  }
  else {
    updateFlags.fetch_or(1u << config::toIndex(parameterID));
  }
}

void QuasarEQAudioProcessor::updateBands(uint32_t flags) {
  const float sr = getSampleRate();
  for (int i = 0; i < config::BAND_COUNT; ++i) {
    if (flags & (1u << i)) {
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
  }
}

void QuasarEQAudioProcessor::update_global() {
  auto loadGlobal = [this](const juce::String& id) {
    return zlth::unit::dbToMag(apvts.getRawParameterValue(id)->load()) * 0.5f;
  };
  gains[0].set_gain(loadGlobal(config::ID_OUT_GAIN_0));
  gains[1].set_gain(loadGlobal(config::ID_OUT_GAIN_1));
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
