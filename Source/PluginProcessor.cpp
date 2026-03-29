#include "PluginProcessor.h"
#include "PluginEditor.h"

QuasarEQAudioProcessor::QuasarEQAudioProcessor():
    AudioProcessor(
        BusesProperties().
        withInput("Input", juce::AudioChannelSet::stereo(), true).
        withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, &undoManager, ID_PARAMETERS, createParameterLayout())
{
    apvts.addParameterListener(ID_GAIN_MID, this);
    apvts.addParameterListener(ID_GAIN_SIDE, this);
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        for (const auto& prefix : bandParamPrefixes)
        {
            apvts.addParameterListener(Params::getID(prefix, i), this);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuasarEQAudioProcessor();
}

juce::AudioProcessorEditor* QuasarEQAudioProcessor::createEditor()
{
    return new QuasarEQAudioProcessorEditor(*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout QuasarEQAudioProcessor::createParameterLayout() const
{
    juce::NormalisableRange<float> gainRange {config::PARAM_GAIN_MIN, config::PARAM_GAIN_MAX, config::PARAM_GAIN_INTERVAL};
    juce::NormalisableRange<float> freqRange {config::PARAM_FREQ_MIN, config::PARAM_FREQ_MAX, config::PARAM_FREQ_INTERVAL};
    juce::NormalisableRange<float> qualRange {config::PARAM_QUAL_MIN, config::PARAM_QUAL_MAX, config::PARAM_QUAL_INTERVAL};

    static const juce::String UNIT_HZ {"Hz"};
    static const juce::String UNIT_DB {"dB"};

    freqRange.setSkewForCentre(freqRange.snapToLegalValue(config::PARAM_FREQ_CENTER));
    qualRange.setSkewForCentre(qualRange.snapToLegalValue(config::PARAM_QUAL_CENTER));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::NormalisableRange<float> abc {config::PARAM_GAIN_MIN, config::PARAM_GAIN_MAX, 1};
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID_GAIN_MID, ID_GAIN_MID, abc, config::PARAM_GAIN_CENTER, UNIT_DB));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID_GAIN_SIDE, ID_GAIN_SIDE, abc, config::PARAM_GAIN_CENTER, UNIT_DB));

    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        const auto id = [i](auto prefix) { return Params::getID(prefix, i); };
        layout.add(
            std::make_unique<juce::AudioParameterFloat>(id(ID_PREFIX_GAIN), id(ID_PREFIX_GAIN), gainRange, config::PARAM_GAIN_CENTER, UNIT_DB),
            std::make_unique<juce::AudioParameterFloat>(id(ID_PREFIX_FREQ), id(ID_PREFIX_FREQ), freqRange, freqRange.snapToLegalValue(config::PARAM_FREQ_CENTER), UNIT_HZ),
            std::make_unique<juce::AudioParameterFloat>(id(ID_PREFIX_QUAL), id(ID_PREFIX_QUAL), qualRange, qualRange.snapToLegalValue(config::PARAM_QUAL_CENTER)),
            std::make_unique<juce::AudioParameterChoice>(id(ID_PREFIX_TYPE), id(ID_PREFIX_TYPE), filterTags, config::PARAM_TYPE_DEFAULT),
            std::make_unique<juce::AudioParameterChoice>(id(ID_PREFIX_MODE), id(ID_PREFIX_MODE), channelModes, config::PARAM_MODE_DEFAULT),
            std::make_unique<juce::AudioParameterBool>(id(ID_PREFIX_BYPASS), id(ID_PREFIX_BYPASS), config::PARAM_BYPASS_DEFAULT)
        );
    }
    return layout;
}

void QuasarEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {};
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = (juce::uint32)getTotalNumOutputChannels();
    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);
    updateFilters(ALL_UPDATE_MASK);
}

bool QuasarEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();
    return mainInput == mainOutput && (mainInput == juce::AudioChannelSet::mono() || mainInput == juce::AudioChannelSet::stereo());
}

void QuasarEQAudioProcessor::updateFilters(uint32_t flags)
{
    if (flags == 0) return;
    const auto sr = getSampleRate();
    for (int i = 0; i < config::BAND_COUNT; ++i)
    {
        if (shouldUpdateBand(flags, i))
        {
            auto loadBandParam = [this, i](const juce::String& prefix)
            {
                return apvts.getRawParameterValue(Params::getID(prefix, i))->load();
            };
            const auto f = loadBandParam(ID_PREFIX_FREQ);
            const auto q = loadBandParam(ID_PREFIX_QUAL);
            const auto gain = loadBandParam(ID_PREFIX_GAIN);
            const auto type = static_cast<zlth::dsp::filter::ZdfSvf2ndOrder::FilterType>((int)loadBandParam(ID_PREFIX_TYPE));
            const auto mode = (int)loadBandParam(ID_PREFIX_MODE);
            const bool bypassed = loadBandParam(ID_PREFIX_BYPASS) > 0.5f;
            bandsM[i].update_coefficients(type, f, q, gain, sr);
            bandsS[i].update_coefficients(type, f, q, gain, sr);
            isBandMActive[i] = !bypassed && (mode == 0 || mode == 1);
            isBandSActive[i] = !bypassed && (mode == 0 || mode == 2);
        }
    }
    if (shouldUpdateGlobal(flags))
    {
        auto loadGlobal = [this](const juce::String& id) {
            return std::pow(10.0f, apvts.getRawParameterValue(id)->load() / 20.0f);
        };
        globalGainLinearMid = loadGlobal(ID_GAIN_MID);
        globalGainLinearSide = loadGlobal(ID_GAIN_SIDE);
    }
}

void QuasarEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    apvts.state.writeToStream(stream);
}

void QuasarEQAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == ID_GAIN_MID || parameterID == ID_GAIN_SIDE)
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

void QuasarEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

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
