#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    static const juce::String UNIT_HZ {"Hz"};
    static const juce::String UNIT_DB {"dB"};

    static constexpr bool BYPASS_DEFAULT = true;

    static constexpr int TYPE_DEFAULT = 4;

    static constexpr float FREQ_START = 20.0f;
    static constexpr float FREQ_END = 20000.0f;
    static constexpr float FREQ_INTERVAL = 0.1f;
    static const float FREQ_CENTRE = sqrt(FREQ_START * FREQ_END);

    static constexpr float GAIN_START = -24.0f;
    static constexpr float GAIN_END = 24.0f;
    static constexpr float GAIN_INTERVAL = 0.01f;
    static constexpr float GAIN_CENTRE = 0.0f;

    static constexpr float QUAL_START = 0.05f;
    static constexpr float QUAL_END = 12.0f;
    static constexpr float QUAL_INTERVAL = 0.001f;
    static constexpr float QUAL_CENTRE = 1.0f / juce::MathConstants<float>::sqrt2;

    juce::NormalisableRange<float> gainRange {GAIN_START, GAIN_END, GAIN_INTERVAL};
    juce::NormalisableRange<float> freqRange {FREQ_START, FREQ_END, FREQ_INTERVAL};
    juce::NormalisableRange<float> qualRange {QUAL_START, QUAL_END, QUAL_INTERVAL};
    freqRange.setSkewForCentre(freqRange.snapToLegalValue(FREQ_CENTRE));
    qualRange.setSkewForCentre(qualRange.snapToLegalValue(QUAL_CENTRE));
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID_GAIN, ID_GAIN, gainRange, GAIN_CENTRE, UNIT_DB));
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        const auto id = [i](auto prefix) { return Params::getID(prefix, i); };
        layout.add(std::make_unique<juce::AudioParameterBool>  (id(ID_PREFIX_BYPASS), id(ID_PREFIX_BYPASS), BYPASS_DEFAULT));
        layout.add(std::make_unique<juce::AudioParameterFloat> (id(ID_PREFIX_GAIN), id(ID_PREFIX_GAIN), gainRange, GAIN_CENTRE, UNIT_DB));
        layout.add(std::make_unique<juce::AudioParameterFloat> (id(ID_PREFIX_FREQ), id(ID_PREFIX_FREQ), freqRange, freqRange.snapToLegalValue(FREQ_CENTRE), UNIT_HZ));
        layout.add(std::make_unique<juce::AudioParameterFloat> (id(ID_PREFIX_QUAL), id(ID_PREFIX_QUAL), qualRange, qualRange.snapToLegalValue(QUAL_CENTRE)));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(ID_PREFIX_TYPE), id(ID_PREFIX_TYPE), filterTags, TYPE_DEFAULT));
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
    outGain.prepare(spec);
    outGain.reset();
    updateFilters(ALL_UPDATE_MASK);
}

bool QuasarEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();
    return mainInput == mainOutput && (mainInput == juce::AudioChannelSet::mono() || mainInput == juce::AudioChannelSet::stereo());
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
        updateFilters(flags);

    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            filters[i].process(leftChannel[s], rightChannel[s]);
        }
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    outGain.process(context);
    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
}

void QuasarEQAudioProcessor::updateFilters(uint32_t flags)
{
    if (flags == 0) return;
    const auto sr = getSampleRate();
    for (int i = 0; i < NUM_BANDS; ++i)
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
            const auto type = static_cast<zlth::dsp::filter::ZdfSvf2ndOrder::Type>((int)loadBandParam(ID_PREFIX_TYPE));

            filters[i].left.update_coefficients(type, f, q, gain, sr);
            filters[i].right.update_coefficients(type, f, q, gain, sr);
            filters[i].bypassed = loadBandParam(ID_PREFIX_BYPASS) > 0.5f;
        }
    }
    if (shouldUpdateGlobal(flags))
    {
        const auto g = apvts.getRawParameterValue(ID_GAIN)->load();
        outGain.get<0>().setGainDecibels(g);
    }
}

void QuasarEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    apvts.state.writeToStream(stream);
}

void QuasarEQAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == ID_GAIN)
    {
        updateFlags.fetch_or(GLOBAL_PARAMS_MASK);
    }
    else
    {
        const int i = Params::getBandIndex(parameterID);
        if (i >= 0 && i < NUM_BANDS)
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

QuasarEQAudioProcessor::QuasarEQAudioProcessor():
    AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, &undoManager, ID_PARAMETERS, createParameterLayout())
{
    apvts.addParameterListener(ID_GAIN, this);
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        for (const auto& prefix : bandParamPrefixes)
        {
            apvts.addParameterListener(Params::getID(prefix, i), this);
        }
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
void QuasarEQAudioProcessor::releaseResources()  {}
void QuasarEQAudioProcessor::setCurrentProgram(int index)  {}
void QuasarEQAudioProcessor::changeProgramName(int index, const juce::String& newName)  {}
bool QuasarEQAudioProcessor::shouldUpdateBand(uint32_t flags, int bandIdx) const { return (flags & (1u << bandIdx)); }
bool QuasarEQAudioProcessor::shouldUpdateGlobal(uint32_t flags) const { return (flags & GLOBAL_PARAMS_MASK) != 0; }

std::vector<SvfParams> QuasarEQAudioProcessor::getSvfParams() const
{
    std::vector<SvfParams> params;
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        auto bypass = apvts.getRawParameterValue(Params::getID(ID_PREFIX_BYPASS, i))->load();
        if (bypass < 0.5f)
        {
            params.push_back({
                static_cast<zlth::dsp::filter::ZdfSvf2ndOrder::Type>((int)apvts.getRawParameterValue(Params::getID(ID_PREFIX_TYPE, i))->load()),
                apvts.getRawParameterValue(Params::getID(ID_PREFIX_FREQ, i))->load(),
                apvts.getRawParameterValue(Params::getID(ID_PREFIX_QUAL, i))->load(),
                apvts.getRawParameterValue(Params::getID(ID_PREFIX_GAIN, i))->load()
                             });
        }
    }
    return params;
}
