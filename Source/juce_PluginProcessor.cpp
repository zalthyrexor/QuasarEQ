#include "juce_PluginProcessor.h"
#include "juce_PluginEditor.h"

QuasarEQAudioProcessor::QuasarEQAudioProcessor():
    AudioProcessor(BusesProperties().
        withInput("Input", juce::AudioChannelSet::stereo(), true).
        withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, &undoManager, config::ID_PARAMETERS, createParameterLayout()) {
    apvts.addParameterListener(config::ID_OUT_GAIN_0, this);
    apvts.addParameterListener(config::ID_OUT_GAIN_1, this);
    for (int i = 0; i < config::BAND_COUNT; ++i) {
        for (const auto& prefix : config::bandParamPrefixes) {
            apvts.addParameterListener(config::getID(prefix, i), this);
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout QuasarEQAudioProcessor::createParameterLayout() const {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::NormalisableRange<float> outGainRange {config::PARAM_OUT_GAIN_MIN, config::PARAM_OUT_GAIN_MAX, config::PARAM_OUT_GAIN_INTERVAL};
    juce::NormalisableRange<float> bandGainRange {config::PARAM_BAND_GAIN_MIN, config::PARAM_BAND_GAIN_MAX, config::PARAM_BAND_GAIN_INTERVAL};
    juce::NormalisableRange<float> bandFreqRange {config::PARAM_BAND_FREQ_MIN, config::PARAM_BAND_FREQ_MAX, config::PARAM_BAND_FREQ_INTERVAL};
    juce::NormalisableRange<float> bandQualRange {config::PARAM_BAND_QUAL_MIN, config::PARAM_BAND_QUAL_MAX, config::PARAM_BAND_QUAL_INTERVAL};
    auto outGainLegalValue = outGainRange.snapToLegalValue(config::PARAM_OUT_GAIN_CENTER);
    auto gainLegalValue = bandGainRange.snapToLegalValue(config::PARAM_BAND_GAIN_CENTER);
    auto freqLegalValue = bandFreqRange.snapToLegalValue(config::PARAM_BAND_FREQ_CENTER);
    auto qualLegalValue = bandQualRange.snapToLegalValue(config::PARAM_BAND_QUAL_CENTER);
    bandFreqRange.setSkewForCentre(freqLegalValue);
    bandQualRange.setSkewForCentre(qualLegalValue);
    layout.add(std::make_unique<juce::AudioParameterFloat>(config::ID_OUT_GAIN_0, config::ID_OUT_GAIN_0, outGainRange, outGainLegalValue, config::bandUnits[0]));
    layout.add(std::make_unique<juce::AudioParameterFloat>(config::ID_OUT_GAIN_1, config::ID_OUT_GAIN_1, outGainRange, outGainLegalValue, config::bandUnits[0]));
    for (int i = 0; i < config::BAND_COUNT; ++i) {
        const auto id = [i](auto prefix) { return config::getID(prefix, i); };
        layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_GAIN), id(config::ID_BAND_GAIN), bandGainRange, gainLegalValue, config::bandUnits[0]));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_FREQ), id(config::ID_BAND_FREQ), bandFreqRange, freqLegalValue, config::bandUnits[1]));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id(config::ID_BAND_QUAL), id(config::ID_BAND_QUAL), bandQualRange, qualLegalValue, config::bandUnits[2]));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(config::ID_BAND_FILTER), id(config::ID_BAND_FILTER), config::filterModes, config::PARAM_BAND_FILTER_DEFAULT));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(config::ID_BAND_CHANNEL), id(config::ID_BAND_CHANNEL), config::channelModes, config::PARAM_BAND_CHANNEL_DEFAULT));
        layout.add(std::make_unique<juce::AudioParameterBool>(id(config::ID_BAND_BYPASS), id(config::ID_BAND_BYPASS), config::PARAM_BAND_BYPASS_DEFAULT));
    }
    return layout;
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
    std::span<float> span0 {buffer.getWritePointer(0), numSamples};
    std::span<float> span1 {buffer.getWritePointer(1), numSamples};
    zlth::simd::hadamard_butterfly(span0, span1);
    processors[0].process(span0);
    processors[1].process(span1);
    channelFifo[0].update(span0);
    channelFifo[1].update(span1);
    zlth::simd::hadamard_butterfly(span0, span1);
}

void QuasarEQAudioProcessor::parameterChanged(const juce::String& parameterID, float) {
    if (parameterID == config::ID_OUT_GAIN_0 || parameterID == config::ID_OUT_GAIN_1) {
        updateFlags.fetch_or(PARAMS_MASK_OUT);
    }
    else {
        updateFlags.fetch_or(1u << config::getBandIndex(parameterID));
    }
}

void QuasarEQAudioProcessor::coefsSetter(zlth::dsp::Filter& filter, int i, float sampleRate) {
    auto loadBandParam = [this, i](const juce::String& prefix) {
        return apvts.getRawParameterValue(config::getID(prefix, i))->load();
    };
    auto qual = loadBandParam(config::ID_BAND_QUAL);
    auto freq = loadBandParam(config::ID_BAND_FREQ);
    auto gain = loadBandParam(config::ID_BAND_GAIN);
    auto type = static_cast<zlth::dsp::Filter::FilterType>((int)loadBandParam(config::ID_BAND_FILTER));
    filter.set_coefficients(type, freq, qual, gain, sampleRate);
}

void QuasarEQAudioProcessor::updateBands(uint32_t flags) {
    const auto sampleRate = getSampleRate();
    for (int i = 0; i < config::BAND_COUNT; ++i) {
        if (flags & 1u << i) {
            coefsSetter(processors[0].bands[i], i, sampleRate);
            coefsSetter(processors[1].bands[i], i, sampleRate);
            auto loadBandParam = [this, i](const juce::String& prefix) {
                return apvts.getRawParameterValue(config::getID(prefix, i))->load();
            };
            auto mode = (int)loadBandParam(config::ID_BAND_CHANNEL);
            bool isActive = loadBandParam(config::ID_BAND_BYPASS) < 0.5f;
            processors[0].isBandActive[i] = isActive && (mode == 0 || mode == 1);
            processors[1].isBandActive[i] = isActive && (mode == 0 || mode == 2);
        }
    }
    if (flags & PARAMS_MASK_OUT) {
        auto loadGlobal = [this](const juce::String& id) {
            return std::pow(10.0f, apvts.getRawParameterValue(id)->load() / 20.0f) * 0.5f;
        };
        processors[0].globalGain = loadGlobal(config::ID_OUT_GAIN_0);
        processors[1].globalGain = loadGlobal(config::ID_OUT_GAIN_1);
    }
}

std::vector<FilterSnapshot> QuasarEQAudioProcessor::getFilterSnapshots() {
    std::vector<FilterSnapshot> snapshots {};
    snapshots.reserve(config::BAND_COUNT);
    float sampleRate = static_cast<float>(getSampleRate());
    for (int i = 0; i < config::BAND_COUNT; ++i) {
        auto load = [this, i](const juce::String& prefix) {
            return apvts.getRawParameterValue(config::getID(prefix, i))->load();
        };
        if (load(config::ID_BAND_BYPASS) > 0.5f) continue;
        FilterSnapshot snapshot {};
        coefsSetter(snapshot.filter, i, sampleRate);
        snapshot.channelMode = (int)load(config::ID_BAND_CHANNEL);
        snapshots.push_back(snapshot);
    }
    return snapshots;
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QuasarEQAudioProcessor(); }
juce::AudioProcessorEditor* QuasarEQAudioProcessor::createEditor() { return new QuasarEQAudioProcessorEditor(*this); }
int QuasarEQAudioProcessor::getNumPrograms() { return 1; }
int QuasarEQAudioProcessor::getCurrentProgram() { return 0; }
bool QuasarEQAudioProcessor::hasEditor() const { return true; }
bool QuasarEQAudioProcessor::acceptsMidi() const { return false; }
bool QuasarEQAudioProcessor::producesMidi() const { return false; }
bool QuasarEQAudioProcessor::isMidiEffect() const { return false; }
double QuasarEQAudioProcessor::getTailLengthSeconds() const { return 0.0; }
const juce::String QuasarEQAudioProcessor::getName() const { return JucePlugin_Name; }
const juce::String QuasarEQAudioProcessor::getProgramName(int index) { return {}; }
void QuasarEQAudioProcessor::releaseResources() {}
void QuasarEQAudioProcessor::setCurrentProgram(int index) {}
void QuasarEQAudioProcessor::changeProgramName(int index, const juce::String& newName) {}
