#pragma once

#include <JuceHeader.h>
#include "zlth_fifo.h"
#include "zlth_dsp_filter_tpt2pole.h"
#include "config.h"
#include "EQProcessor.h"

struct FilterSnapshot
{
    zlth::dsp::filter::TPT2Pole filter {};
    int channelMode = 0;
};

static inline const juce::String ID_PARAMETERS {"PARAMETERS"};
static inline const juce::String ID_OUT_GAIN_0 {"ID_OUT_GAIN_MID"};
static inline const juce::String ID_OUT_GAIN_1 {"ID_OUT_GAIN_SIDE"};
static inline const juce::String ID_BAND_FREQ {"FREQ"};
static inline const juce::String ID_BAND_GAIN {"GAIN"};
static inline const juce::String ID_BAND_QUAL {"QUAL"};
static inline const juce::String ID_BAND_FILTER {"TYPE"};
static inline const juce::String ID_BAND_BYPASS {"BYPASS"};
static inline const juce::String ID_BAND_CHANNEL {"MODE"};
static inline const juce::StringArray filterModes {"HIGHPASS", "LOWPASS", "HIGHSHELF", "LOWSHELF", "TILT", "BELL", "NOTCH", "BANDPASS"};
static inline const juce::StringArray channelModes {"STEREO", "MID", "SIDE"};
static inline const juce::StringArray bandParamPrefixes {ID_BAND_FREQ, ID_BAND_GAIN, ID_BAND_QUAL, ID_BAND_FILTER, ID_BAND_BYPASS, ID_BAND_CHANNEL};

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
    SingleChannelSampleFifo channelFifo0 {Channel::Left};
    SingleChannelSampleFifo channelFifo1 {Channel::Right};
    std::vector<FilterSnapshot> getFilterSnapshots() const;
private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() const;
    void updateBands(uint32_t flags);

    static constexpr uint32_t PARAMS_MASK_BAND = (1u << config::BAND_COUNT) - 1;
    static constexpr uint32_t PARAMS_MASK_OUT = (1u << config::BAND_COUNT + config::OUT_GAIN_COUNT) - 1;
    static constexpr uint32_t PARAMS_MASK_ALL = PARAMS_MASK_BAND | PARAMS_MASK_OUT;
    std::atomic<uint32_t> updateFlags {PARAMS_MASK_ALL};

    std::array<ProcessChain<config::BAND_COUNT>, 2> processors {};
    std::array<float, 2> globalGains {};

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
