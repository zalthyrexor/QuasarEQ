#include "juce_PluginProcessor.h"
#include "juce_PluginEditor.h"
#include "unit.h"

QuasarEQAudioProcessor::QuasarEQAudioProcessor():AudioProcessor(BusesProperties().
  withInput("Input", juce::AudioChannelSet::stereo(), true).
  withOutput("Output", juce::AudioChannelSet::stereo(), true)), apvts(*this, &undoManager, config::ID_PARAMETERS, createParameterLayout()) {

  processors.push_back({
    zlth::dsp::Gain(apvts.getRawParameterValue(config::ID_OUT_GAIN[0])),
    zlth::dsp::Gain(apvts.getRawParameterValue(config::ID_OUT_GAIN[1]))
    }
  );

  for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
    auto* l0_ = apvts.getRawParameterValue(config::toBiquadID(config::biquadPrefixes[(int)config::BandAddressEnum::freq], i));
    auto* l1_ = apvts.getRawParameterValue(config::toBiquadID(config::biquadPrefixes[(int)config::BandAddressEnum::q], i));
    auto* l2_ = apvts.getRawParameterValue(config::toBiquadID(config::biquadPrefixes[(int)config::BandAddressEnum::gain], i));
    auto* l3_ = apvts.getRawParameterValue(config::toBiquadID(config::biquadPrefixes[(int)config::BandAddressEnum::bypass], i));
    auto* l4_ = apvts.getRawParameterValue(config::toBiquadID(config::biquadPrefixes[(int)config::BandAddressEnum::shape], i));
    auto* l5_ = apvts.getRawParameterValue(config::toBiquadID(config::biquadPrefixes[(int)config::BandAddressEnum::channel], i));
    processors.push_back({
      zlth::dsp::Filter(&sampleRate, l0_, l1_, l2_, l3_, l4_, l5_, 0),
      zlth::dsp::Filter(&sampleRate, l0_, l1_, l2_, l3_, l4_, l5_, 1)
      }
    );
  }

  for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
    for (int j = 0; j < config::biquadPrefixCount; ++j) {
      biquadTable[i][j] = apvts.getRawParameterValue(config::toBiquadID(config::biquadPrefixes[j], i));
    }
  }
  for (int i = 0; i < config::BUTTER_COUNT; ++i) {
    for (int j = 0; j < config::butterPrefixCount; ++j) {
      butterTable[i][j] = apvts.getRawParameterValue(config::toButterID(config::butterPrefixes[j], i));
    }
  }
}

void QuasarEQAudioProcessor::initializeAllParameters() const {
  for (int i = 0; i < config::CHANNEL_COUNT; ++i) {
    resetParam(config::ID_OUT_GAIN[i]);
  }
  for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
    for (int j = 0; j < config::biquadPrefixCount; ++j) {
      resetParam(config::toBiquadID(config::biquadPrefixes[j], i));
    }
  }
  for (int i = 0; i < config::BUTTER_COUNT; ++i) {
    for (int j = 0; j < config::butterPrefixCount; ++j) {
      resetParam(config::toButterID(config::butterPrefixes[j], i));
    }
  }
}

juce::AudioProcessorValueTreeState::ParameterLayout QuasarEQAudioProcessor::createParameterLayout() const {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  auto logFrom0To1 = [](float start, float end, float proportion) {
    return start * std::pow(end / start, proportion);
  };
  auto logTo0To1 = [](float start, float end, float v) {
    return std::log(v / start) / std::log(end / start);
  };
  juce::NormalisableRange<float> gainRange {config::PARAM_GAIN_DB_MIN, config::PARAM_GAIN_DB_MAX};
  juce::NormalisableRange<float> freqRange {config::PARAM_FREQ_HZ_MIN, config::PARAM_FREQ_HZ_MAX, logFrom0To1, logTo0To1};
  juce::NormalisableRange<float> qualRange {config::PARAM_Q_MIN, config::PARAM_Q_MAX, logFrom0To1, logTo0To1};
  juce::NormalisableRange<float> orderRange {config::PARAM_ORDER_MIN, config::PARAM_ORDER_MAX, 1.0f};
  auto makeAttrs = [](int decimals, const juce::String& label) {
    return juce::AudioParameterFloatAttributes()
      .withStringFromValueFunction([decimals](float v, int) { return juce::String(v, decimals); })
      .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); })
      .withLabel(label);
  };
  auto gainAttrs = makeAttrs(2, config::UNIT_GAIN);
  auto freqAttrs = makeAttrs(2, config::UNIT_FREQ);
  auto qualAttrs = makeAttrs(2, config::UNIT_Q);
  auto orderAttrs = makeAttrs(0, config::UNIT_ORDER);
  auto addBool = [&](auto paramID) {
    layout.add(std::make_unique<juce::AudioParameterBool>(paramID, paramID, config::PARAM_BYPASS_DEFAULT));
  };
  for (int i = 0; i < config::CHANNEL_COUNT; ++i) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(config::ID_OUT_GAIN[i], config::ID_OUT_GAIN[i], gainRange, config::PARAM_GAIN_DB_DEF, gainAttrs));
  }
  auto addChoice = [&](auto paramID, const juce::StringArray& choices, int defaultIdx) {
    layout.add(std::make_unique<juce::AudioParameterChoice>(paramID, paramID, choices, defaultIdx));
  };
  for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
    const auto id = [i](auto prefix) { return config::toBiquadID(prefix, i); };
    float proportion = static_cast<float>(i + 2) / static_cast<float>(config::BIQUAD_COUNT + 3);
    float initialFreq = config::PARAM_FREQ_HZ_MIN * std::pow(config::PARAM_FREQ_HZ_MAX / config::PARAM_FREQ_HZ_MIN, proportion);
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_GAIN), id(config::ID_GAIN), gainRange, config::PARAM_GAIN_DB_DEF, gainAttrs));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_FREQ), id(config::ID_FREQ), freqRange, initialFreq, freqAttrs));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_Q), id(config::ID_Q), qualRange, config::PARAM_Q_DEF, qualAttrs));
    addChoice(id(config::ID_FILTER_SHAPE), config::filterModeStrings, config::PARAM_FILTER_DEFAULT);
    addChoice(id(config::ID_CHANNEL_MODE), config::channelModeStrings, config::PARAM_CHANNEL_DEFAULT);
    addBool(id(config::ID_BYPASS));
  }
  for (int i = 0; i < config::BUTTER_COUNT; ++i) {
    const auto id = [i](auto prefix) { return config::toButterID(prefix, i); };
    float initialFreq = 500.0f;
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_FREQ), id(config::ID_FREQ), freqRange, initialFreq, freqAttrs));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_ORDER), id(config::ID_ORDER), orderRange, config::PARAM_ORDER_DEF, orderAttrs));
    addChoice(id(config::ID_BUTTER_SHAPE), config::ButterShapeStrings, config::PARAM_BUTTER_DEFAULT);
    addChoice(id(config::ID_CHANNEL_MODE), config::channelModeStrings, config::PARAM_CHANNEL_DEFAULT);
    addBool(id(config::ID_BYPASS));
  }
  return layout;
}

void QuasarEQAudioProcessor::resetParam (const juce::String& id) const{
  if (auto* p = apvts.getParameter(id)) {
    p->setValueNotifyingHost(p->getDefaultValue());
  }
}

void QuasarEQAudioProcessor::prepareToPlay(double sampleRate_, int samplesPerBlock) {
  sampleRate.store(sampleRate_);
  for (int i = 0; i < config::CHANNEL_COUNT; ++i) {
    channelFifo[i].prepare(samplesPerBlock);
  }
}

void QuasarEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
    buffer.clear(i, 0, buffer.getNumSamples());
  }
  const auto numSamples = static_cast<size_t>(buffer.getNumSamples());
  std::span<float> span[] {{buffer.getWritePointer(0), numSamples}, {buffer.getWritePointer(1), numSamples}};
  zlth::simd::hadamard_butterfly(span[0], span[1]);
  for (int i = 0; i < config::CHANNEL_COUNT; ++i) {
    zlth::simd::mul_inplace(span[i], 0.5f);
  }
  for (auto& p : processors) {
    for (int j = 0; j < config::CHANNEL_COUNT; ++j) {
      std::visit([&](auto& arg) {arg.process(span[j]); }, p[j]);
    }
  }
  for(int i = 0; i < config::CHANNEL_COUNT; ++i){
    channelFifo[i].update(span[i]);
  }
  zlth::simd::hadamard_butterfly(span[0], span[1]);
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
