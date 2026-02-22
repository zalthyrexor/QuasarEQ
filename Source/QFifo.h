#pragma once

#include <array>
#include <JuceHeader.h>
template <typename T>
struct Fifo
{
    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            buffers[write.startIndex1] = t;
            return true;
        }
        return false;
    }
    bool pull(T& t)
    {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffers[read.startIndex1];
            return true;
        }
        return false;
    }
    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }
private:
    std::array<T, 32> buffers;
    juce::AbstractFifo fifo{32};
    std::mutex mutex;
};
template <int Capacity>
struct AudioBufferFifo
{
    void prepare(const int numChannels, const int numSamples)
    {
        for (auto& buffer : buffers)
        {
            buffer.setSize(numChannels, numSamples, false, true, true);
            buffer.clear();
        }
    }
    bool push(const juce::AudioBuffer<float>& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            buffers[write.startIndex1] = t;
            return true;
        }
        return false;
    }
    bool pull(juce::AudioBuffer<float>& t)
    {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffers[read.startIndex1];
            return true;
        }
        return false;
    }
    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }
private:
    std::array<juce::AudioBuffer<float>, Capacity> buffers;
    juce::AbstractFifo fifo{Capacity};
};
enum Channel
{
    Left, Right
};
struct SingleChannelSampleFifo
{
    SingleChannelSampleFifo(Channel ch): channelToUse(ch)
    {
    }
    void update(const juce::AudioBuffer<float>& buffer)
    {
        auto* channelPtr = buffer.getReadPointer(channelToUse);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (fifoIndex == bufferToFill.getNumSamples())
            {
                juce::ignoreUnused(audioBufferFifo.push(bufferToFill));
                fifoIndex = 0;
            }
            bufferToFill.setSample(0, fifoIndex, channelPtr[i]);
            ++fifoIndex;
        }
    }
    void prepare(int bufferSize)
    {
        bufferToFill.setSize(1, bufferSize, false, true, true);
        audioBufferFifo.prepare(1, bufferSize);
        fifoIndex = 0;
    }
    int getNumCompleteBuffersAvailable() const
    {
        return audioBufferFifo.getNumAvailableForReading();
    }
    bool getAudioBuffer(juce::AudioBuffer<float>& buf)
    {
        return audioBufferFifo.pull(buf);
    }
private:
    Channel channelToUse;
    int fifoIndex = 0;
    AudioBufferFifo<16> audioBufferFifo;
    juce::AudioBuffer<float> bufferToFill;
};
