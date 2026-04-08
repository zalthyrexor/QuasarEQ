#include "juce_PluginProcessor.h"
#include "juce_PluginEditor.h"

QuasarEQAudioProcessor::QuasarEQAudioProcessor():
    AudioProcessor(
        BusesProperties().
        withInput("Input", juce::AudioChannelSet::stereo(), true).
        withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, &undoManager, ID_PARAMETERS, createParameterLayout())
{
    apvts.addParameterListener(ID_OUT_GAIN_0, this);
    apvts.addParameterListener(ID_OUT_GAIN_1, this);
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
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID_OUT_GAIN_0, ID_OUT_GAIN_0, outGainRange, outGainLegalValue, UNIT_DB));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID_OUT_GAIN_1, ID_OUT_GAIN_1, outGainRange, outGainLegalValue, UNIT_DB));
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
    channelFifo0.prepare(samplesPerBlock);
    channelFifo1.prepare(samplesPerBlock);
    updateBands(ALL_UPDATE_MASK);
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
        updateBands(flags);
    }
    const int numSamples = buffer.getNumSamples();
    std::span<float> span0 {buffer.getWritePointer(0), static_cast<size_t>(numSamples)};
    std::span<float> span1 {buffer.getWritePointer(1), static_cast<size_t>(numSamples)};
    zlth::simd::hadamard_butterfly(span0, span1);
	zlth::simd::multiply_inplace(span0, globalGainLinear0 * 0.5f);
    zlth::simd::multiply_inplace(span1, globalGainLinear1 * 0.5f);
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        if (isBand0Active[i]) 
        {
            bands0[i].process_span(span0);
        }
        if (isBand1Active[i]) 
        {
            bands1[i].process_span(span1);
        }
    }
    channelFifo0.update(buffer);
    channelFifo1.update(buffer);
    zlth::simd::hadamard_butterfly(span0, span1);
}

void QuasarEQAudioProcessor::updateBands(uint32_t flags)
{
    const auto sampleRate = getSampleRate();
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        if (!(flags & (1u << i))) continue;
        auto loadBandParam = [this, i](const juce::String& prefix)
        {
            return apvts.getRawParameterValue(Params::getID(prefix, i))->load();
        };
        const auto k = 1.0f / std::max(loadBandParam(ID_BAND_QUAL), 0.01f);
        const auto freq = loadBandParam(ID_BAND_FREQ);
        const auto gain = loadBandParam(ID_BAND_GAIN);
        const auto type = static_cast<zlth::dsp::filter::TPT2Pole::FilterType>((int)loadBandParam(ID_BAND_FILTER));
        const auto mode = (int)loadBandParam(ID_BAND_CHANNEL);
        const bool isActive = loadBandParam(ID_BAND_BYPASS) < 0.5f;
        bands0[i].set_coefficients(type, freq, k, gain, sampleRate);
        bands1[i].set_coefficients(type, freq, k, gain, sampleRate);
        isBand0Active[i] = isActive && (mode == 0 || mode == 1);
        isBand1Active[i] = isActive && (mode == 0 || mode == 2);
    }
    if (shouldUpdateGlobal(flags))
    {
        auto loadGlobal = [this](const juce::String& id)
        {
            return std::pow(10.0f, apvts.getRawParameterValue(id)->load() / 20.0f);
        };
        globalGainLinear0 = loadGlobal(ID_OUT_GAIN_0);
        globalGainLinear1 = loadGlobal(ID_OUT_GAIN_1);
    }
}

void QuasarEQAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == ID_OUT_GAIN_0 || parameterID == ID_OUT_GAIN_1)
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
        const auto k = 1.0f / std::max(load(ID_BAND_QUAL), 0.01f);
        auto freq = load(ID_BAND_FREQ);
        auto gain = load(ID_BAND_GAIN);
        auto type = static_cast<zlth::dsp::filter::TPT2Pole::FilterType>((int)load(ID_BAND_FILTER));
        FilterSnapshot snapshot;
        snapshot.filter.set_coefficients(type, freq, k, gain, sampleRate);
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
bool QuasarEQAudioProcessor::shouldUpdateGlobal(uint32_t flags) const { return (flags & GLOBAL_PARAMS_MASK) != 0; }
