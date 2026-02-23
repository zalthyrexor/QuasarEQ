#pragma once
#include <JuceHeader.h>
#include "zlth_fifo.h"
#include "zlth_dsp_filter.h"

struct SvfParams
{
    zlth::dsp::filter::ZdfSvfFilter::Type type;
    float freq, q, gainDb;
};

static inline const juce::String ID_PARAMETERS {"Parameters"};
static inline const juce::String ID_GAIN {"MasterGain"};
static inline const juce::String ID_PREFIX_FREQ {"Freq"};
static inline const juce::String ID_PREFIX_GAIN {"Gain"};
static inline const juce::String ID_PREFIX_QUAL {"Q"};
static inline const juce::String ID_PREFIX_TYPE {"Type"};
static inline const juce::String ID_PREFIX_BYPASS {"Bypass"};

static inline const juce::StringArray filterTags {"LowCut", "HighShelf", "HighCut", "LowShelf", "Peak", "Tilt"};
static inline const juce::StringArray bandParamPrefixes = {ID_PREFIX_FREQ, ID_PREFIX_GAIN, ID_PREFIX_QUAL, ID_PREFIX_TYPE, ID_PREFIX_BYPASS};

static constexpr int NUM_BANDS = 8;

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
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
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
    std::vector<SvfParams> getSvfParams() const;
private:

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

    static constexpr uint32_t ALL_BANDS_MASK = (1u << NUM_BANDS) - 1;
    static constexpr uint32_t GLOBAL_PARAMS_MASK = (1u << NUM_BANDS);
    static constexpr uint32_t ALL_UPDATE_MASK = ALL_BANDS_MASK | GLOBAL_PARAMS_MASK;

    juce::dsp::ProcessorChain<juce::dsp::Gain<float>> outGain;
    std::atomic<uint32_t> updateFlags {ALL_UPDATE_MASK};

    void updateFilters(uint32_t flags);
    bool shouldUpdateBand(uint32_t flags, int bandIdx) const;
    bool shouldUpdateGlobal(uint32_t flags) const;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;

    struct StereoSvf
    {
        zlth::dsp::filter::ZdfSvfFilter left, right;
        bool bypassed = false;

        void process(float& l, float& r)
        {
            if (bypassed) return;
            l = left.process_sample(l);
            r = right.process_sample(r);
        }
    };

    std::array<StereoSvf, NUM_BANDS> filters;
};
