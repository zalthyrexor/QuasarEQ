#include "PluginProcessor.h"
#include "PluginEditor.h"

QuasarEQAudioProcessor::QuasarEQAudioProcessor():
    AudioProcessor(
        BusesProperties().
        withInput("Input", juce::AudioChannelSet::stereo(), true).
        withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, &undoManager, ID_PARAMETERS, createParameterLayout())
{
    apvts.addParameterListener(ID_OUT_GAIN_MID, this);
    apvts.addParameterListener(ID_OUT_GAIN_SIDE, this);
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        for (const auto& prefix : bandParamPrefixes)
        {
            apvts.addParameterListener(Params::getID(prefix, i), this);
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout QuasarEQAudioProcessor::createParameterLayout() const
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::String UNIT_DB = juce::String(config::UNIT_DB.data());
    juce::String UNIT_HZ = juce::String(config::UNIT_HZ.data());
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
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID_OUT_GAIN_MID, ID_OUT_GAIN_MID, outGainRange, outGainLegalValue, UNIT_DB));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID_OUT_GAIN_SIDE, ID_OUT_GAIN_SIDE, outGainRange, outGainLegalValue, UNIT_DB));
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        const auto id = [i](auto prefix) { return Params::getID(prefix, i); };
        layout.add(std::make_unique<juce::AudioParameterFloat>(id(ID_BAND_GAIN), id(ID_BAND_GAIN), bandGainRange, gainLegalValue, UNIT_DB));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id(ID_BAND_FREQ), id(ID_BAND_FREQ), bandFreqRange, freqLegalValue, UNIT_HZ));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id(ID_BAND_QUAL), id(ID_BAND_QUAL), bandQualRange, qualLegalValue));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(ID_BAND_FILTER), id(ID_BAND_FILTER), filterModes, config::PARAM_BAND_FILTER_DEFAULT));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(ID_BAND_CHANNEL), id(ID_BAND_CHANNEL), channelModes, config::PARAM_BAND_CHANNEL_DEFAULT));
        layout.add(std::make_unique<juce::AudioParameterBool>(id(ID_BAND_BYPASS), id(ID_BAND_BYPASS), config::PARAM_BAND_BYPASS_DEFAULT));
    }
    return layout;
}

void QuasarEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);
    updateFilters(ALL_UPDATE_MASK);
}

void QuasarEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    {
        buffer.clear(i, 0, buffer.getNumSamples());
    }
    if (auto flags = updateFlags.exchange(0))
    {
        updateFilters(flags);
    }
    const int numSamples = buffer.getNumSamples();
    auto* channelL = buffer.getWritePointer(0);
    auto* channelR = buffer.getWritePointer(1);
    const float optimizedGainMid = globalGainLinearMid * 0.5f;
    const float optimizedGainSide = globalGainLinearSide * 0.5f;
    for (int i = 0; i < numSamples; ++i)
    {
        float l = channelL[i];
        float r = channelR[i];
        channelL[i] = (l + r) * optimizedGainMid;
        channelR[i] = (l - r) * optimizedGainSide;
    }
    auto processBand = [numSamples](float* data, auto& filter)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            data[s] = static_cast<float>(filter.process_sample(static_cast<double>(data[s])));
        }
    };
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        if (isBandMActive[i]) processBand(channelL, bandsM[i]);
        if (isBandSActive[i]) processBand(channelR, bandsS[i]);
    }
    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
    for (int i = 0; i < numSamples; ++i)
    {
        float m = channelL[i];
        float s = channelR[i];
        channelL[i] = m + s;
        channelR[i] = m - s;
    }
}

void QuasarEQAudioProcessor::updateFilters(uint32_t flags)
{
    if (flags == 0) return;
    const auto sampleRate = getSampleRate();
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        if (!shouldUpdateBand(flags, i)) continue;
        auto loadBandParam = [this, i](const juce::String& prefix)
        {
            return apvts.getRawParameterValue(Params::getID(prefix, i))->load();
        };
        const auto freq = loadBandParam(ID_BAND_FREQ);
        const auto qual = loadBandParam(ID_BAND_QUAL);
        const auto gain = loadBandParam(ID_BAND_GAIN);
        const auto type = static_cast<zlth::dsp::filter::ZdfSvf2ndOrder::FilterType>((int)loadBandParam(ID_BAND_FILTER));
        const auto mode = (int)loadBandParam(ID_BAND_CHANNEL);
        const bool bypassed = loadBandParam(ID_BAND_BYPASS) > 0.5f;
        bandsM[i].update_coefficients(type, freq, qual, gain, sampleRate);
        bandsS[i].update_coefficients(type, freq, qual, gain, sampleRate);
        isBandMActive[i] = !bypassed && (mode == 0 || mode == 1);
        isBandSActive[i] = !bypassed && (mode == 0 || mode == 2);
    }
    if (shouldUpdateGlobal(flags))
    {
        auto loadGlobal = [this](const juce::String& id) {
            return std::pow(10.0f, apvts.getRawParameterValue(id)->load() / 20.0f);
        };
        globalGainLinearMid = loadGlobal(ID_OUT_GAIN_MID);
        globalGainLinearSide = loadGlobal(ID_OUT_GAIN_SIDE);
    }
}

void QuasarEQAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == ID_OUT_GAIN_MID || parameterID == ID_OUT_GAIN_SIDE)
    {
        updateFlags.fetch_or(GLOBAL_PARAMS_MASK);
    }
    else
    {
        const int i = Params::getBandIndex(parameterID);
        if (i >= 0 && i < config::BAND_COUNT)
        {
            updateFlags.fetch_or(1u << i);
        }
    }
}

std::vector<FilterSnapshot> QuasarEQAudioProcessor::getFilterSnapshots() const
{
    std::vector<FilterSnapshot> snapshots;
    snapshots.reserve(config::BAND_COUNT);
    float sampleRate = static_cast<float>(getSampleRate());
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        auto load = [this, i](const juce::String& prefix) 
        {
            return apvts.getRawParameterValue(Params::getID(prefix, i))->load();
        };
        if (load(ID_BAND_BYPASS) > 0.5f) continue;
        auto type = static_cast<zlth::dsp::filter::ZdfSvf2ndOrder::FilterType>((int)load(ID_BAND_FILTER));
        auto freq = load(ID_BAND_FREQ);
        auto qual = load(ID_BAND_QUAL);
        auto gain = load(ID_BAND_GAIN);
        FilterSnapshot snapshot;
        snapshot.filter.update_coefficients(type, freq, qual, gain, sampleRate);
        snapshot.channelMode = (int)load(ID_BAND_CHANNEL);
        snapshots.push_back(snapshot);
    }
    return snapshots;
}

void QuasarEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    apvts.state.writeToStream(stream);
}
void QuasarEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

bool QuasarEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();
    return mainInput == mainOutput && (mainInput == juce::AudioChannelSet::mono() || mainInput == juce::AudioChannelSet::stereo());
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
bool QuasarEQAudioProcessor::shouldUpdateBand(uint32_t flags, int bandIdx) const { return (flags & (1u << bandIdx)); }
bool QuasarEQAudioProcessor::shouldUpdateGlobal(uint32_t flags) const { return (flags & GLOBAL_PARAMS_MASK) != 0; }
