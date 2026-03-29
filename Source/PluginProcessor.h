#pragma once

#include <JuceHeader.h>
#include "zlth_fifo.h"
#include "zlth_dsp_filter.h"

struct FilterSnapshot
{
    zlth::dsp::filter::ZdfSvf2ndOrder filter;
    int channelMode;
};

static constexpr int NUM_BANDS = 8;
static inline const juce::String ID_PARAMETERS {"PARAMETERS"};
static inline const juce::String ID_GAIN {"MasterGain"};
static inline const juce::String ID_PREFIX_MODE {"MODE"};
static inline const juce::String ID_PREFIX_FREQ {"FREQ"};
static inline const juce::String ID_PREFIX_GAIN {"GAIN"};
static inline const juce::String ID_PREFIX_QUAL {"QUAL"};
static inline const juce::String ID_PREFIX_TYPE {"TYPE"};
static inline const juce::String ID_PREFIX_BYPASS {"BYPASS"};
static inline const juce::StringArray filterTags {"HP", "LP", "HS", "LS", "BELL", "TILT", "NOTCH", "BP"};
static inline const juce::StringArray channelModes {"STEREO", "MID", "SIDE"};
static inline const juce::StringArray bandParamPrefixes = {ID_PREFIX_FREQ, ID_PREFIX_GAIN, ID_PREFIX_QUAL, ID_PREFIX_TYPE, ID_PREFIX_BYPASS, ID_PREFIX_MODE};

class QuasarEQAudioProcessor: public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
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
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
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
        const float optimizedGain = globalGainLinear * 0.5f;
        for (int i = 0; i < numSamples; ++i)
        {
            float l = channelL[i];
            float r = channelR[i];
            channelL[i] = (l + r) * optimizedGain;
            channelR[i] = (l - r) * optimizedGain;
        }
        auto processBand = [numSamples](float* data, auto& filter)
        {
            for (int s = 0; s < numSamples; ++s)
            {
                data[s] = static_cast<float>(filter.process_sample(static_cast<double>(data[s])));
            }
        };
        for (int i = 0; i < NUM_BANDS; ++i)
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
    void releaseResources() override;
    void setCurrentProgram(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    void parameterChanged(const juce::String& parameterID, float) override;
    double getTailLengthSeconds() const override;
    const juce::String getName() const override;
    const juce::String getProgramName(int index);
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;
    SingleChannelSampleFifo leftChannelFifo {Channel::Left};
    SingleChannelSampleFifo rightChannelFifo {Channel::Right};
    std::vector<FilterSnapshot> getFilterSnapshots() const
    {
        std::vector<FilterSnapshot> snapshots;
        double sr = getSampleRate();

        for (int i = 0; i < NUM_BANDS; ++i)
        {
            auto bypass = apvts.getRawParameterValue(Params::getID(ID_PREFIX_BYPASS, i))->load();
            if (bypass < 0.5f)
            {
                FilterSnapshot s;
                s.filter.update_coefficients(
                    static_cast<zlth::dsp::filter::ZdfSvf2ndOrder::FilterType>((int)apvts.getRawParameterValue(Params::getID(ID_PREFIX_TYPE, i))->load()),
                    apvts.getRawParameterValue(Params::getID(ID_PREFIX_FREQ, i))->load(),
                    apvts.getRawParameterValue(Params::getID(ID_PREFIX_QUAL, i))->load(),
                    apvts.getRawParameterValue(Params::getID(ID_PREFIX_GAIN, i))->load(),
                    (float)sr);
                s.channelMode = (int)apvts.getRawParameterValue(Params::getID(ID_PREFIX_MODE, i))->load();

                snapshots.push_back(s);
            }
        }
        return snapshots;
    }
private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;
    void updateFilters(uint32_t flags);
    bool shouldUpdateBand(uint32_t flags, int bandIdx) const;
    bool shouldUpdateGlobal(uint32_t flags) const;
    static constexpr uint32_t ALL_BANDS_MASK = (1u << NUM_BANDS) - 1;
    static constexpr uint32_t GLOBAL_PARAMS_MASK = (1u << NUM_BANDS);
    static constexpr uint32_t ALL_UPDATE_MASK = ALL_BANDS_MASK | GLOBAL_PARAMS_MASK;
    std::atomic<uint32_t> updateFlags {ALL_UPDATE_MASK};
    std::array<zlth::dsp::filter::ZdfSvf2ndOrder, NUM_BANDS> bandsM;
    std::array<zlth::dsp::filter::ZdfSvf2ndOrder, NUM_BANDS> bandsS;
    std::array<bool, NUM_BANDS> isBandMActive;
    std::array<bool, NUM_BANDS> isBandSActive;
    float globalGainLinear {1.0f};
    struct Params
    {
        static inline juce::String getID(const juce::String& prefix, int bandIdx)
        {
            return prefix + juce::String(bandIdx + 1);
        }
        static inline int getBandIndex(const juce::String& parameterID)
        {
            return parameterID.getTrailingIntValue() - 1;
        }
    };
};
