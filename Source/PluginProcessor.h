#pragma once
#include <JuceHeader.h>
#include "QFifo.h"



static inline const juce::String ID_PARAMETERS {"Parameters"};
static inline const juce::String ID_GAIN {"MasterGain"};
static inline const juce::String ID_PREFIX_FREQ {"Freq"};
static inline const juce::String ID_PREFIX_GAIN {"Gain"};
static inline const juce::String ID_PREFIX_QUAL {"Q"};
static inline const juce::String ID_PREFIX_TYPE {"Type"};
static inline const juce::String ID_PREFIX_BYPASS {"Bypass"};

static inline const juce::StringArray filterTags {"LowCut", "HighShelf", "HighCut", "LowShelf", "Peak"};
static inline const juce::StringArray bandParamPrefixes = {ID_PREFIX_FREQ, ID_PREFIX_GAIN, ID_PREFIX_QUAL, ID_PREFIX_TYPE, ID_PREFIX_BYPASS};

static constexpr int NUM_BANDS = 8;

class QuasarEQAudioProcessor: public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    using T = float;
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
    std::vector<juce::dsp::IIR::Coefficients<T>::Ptr> getCurrentCoefficients() const;
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;
    SingleChannelSampleFifo leftChannelFifo {Channel::Left};
    SingleChannelSampleFifo rightChannelFifo {Channel::Right};
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

    template <typename T>
    static auto getFactory(int index)
    {
        using FilterFactory = typename juce::dsp::IIR::Coefficients<T>::Ptr (*)(double, T, T, T);
        static const FilterFactory factories[] = {
            [](double sr, T f, T q, T) { return juce::dsp::IIR::Coefficients<T>::makeHighPass(sr, f, q); },
            [](double sr, T f, T q, T g) { return juce::dsp::IIR::Coefficients<T>::makeHighShelf(sr, f, q, g); },
            [](double sr, T f, T q, T) { return juce::dsp::IIR::Coefficients<T>::makeLowPass(sr, f, q); },
            [](double sr, T f, T q, T g) { return juce::dsp::IIR::Coefficients<T>::makeLowShelf(sr, f, q, g); },
            [](double sr, T f, T q, T g) { return juce::dsp::IIR::Coefficients<T>::makePeakFilter(sr, f, q, g); }
        };
        return factories[index];
    }


    static constexpr uint32_t ALL_BANDS_MASK = (1u << NUM_BANDS) - 1;
    static constexpr uint32_t GLOBAL_PARAMS_MASK = (1u << NUM_BANDS);
    static constexpr uint32_t ALL_UPDATE_MASK = ALL_BANDS_MASK | GLOBAL_PARAMS_MASK;

    template <size_t... Is>
    static auto make_chain_type(std::index_sequence<Is...>)
        -> juce::dsp::ProcessorChain < std::decay_t<decltype((void)Is, juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<T>, juce::dsp::IIR::Coefficients<T>>{}) > ... > ;
    template <size_t N>
    using FilterChain = decltype(make_chain_type(std::make_index_sequence<N>{}));

    FilterChain<NUM_BANDS> filterChain;
    juce::dsp::ProcessorChain<juce::dsp::Gain<T>> outGain;
    std::atomic<uint32_t> updateFlags {ALL_UPDATE_MASK};

    template <typename SampleType, size_t... Is>
    void updateSpecificFilterImpl(std::index_sequence<Is...>, int targetIndex, typename juce::dsp::IIR::Coefficients<SampleType>::Ptr newCoefs, bool bypassed)
    {
        ([&]
            {
                if (targetIndex == static_cast<int>(Is))
                {
                    *filterChain.get<Is>().state = *newCoefs;
                    filterChain.setBypassed<Is>(bypassed);
                }
            }
        (), ...);
    }

    void updateFilters(uint32_t flags);
    void updateProcessorAtIndex(int index, juce::dsp::IIR::Coefficients<T>::Ptr newCoefs, bool bypassed);
    void markBandForUpdate(int bandIdx);
    void markGlobalParamsForUpdate();
    void markAllForUpdate();
    bool shouldUpdateBand(uint32_t flags, int bandIdx) const;
    bool shouldUpdateGlobal(uint32_t flags) const;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;
};
