#pragma once

#include <vector>
#include "zlth_fifo.h"
#include "zlth_dsp_fft_radix4.h"
#include "zlth_dsp_window.h"
#include "zlth_dsp_window_coefficients.h"
#include "zlth_simd.h"

struct SpectrumRenderData
{
    std::vector<float> spectrumPath;
    std::vector<float> peakHoldPath;
    float dbM = -100.0f;
    float dbS = -100.0f;
};

class PathProducer
{
public:
    PathProducer(SingleChannelSampleFifo& leftScsf, SingleChannelSampleFifo& rightScsf) : channelFifoL(&leftScsf), channelFifoR(&rightScsf)
    {
        decibelsPeak.assign(FFT_SIZE_HALF, -100.0f);
        decibelsCurrent.assign(FFT_SIZE_HALF, 0.0f);

        zlth::dsp::window::fill_window(windowTable, zlth::dsp::window::coefficients::blackman_harris_92);
        const float windowNormalize = static_cast<float>(windowTable.size()) / std::accumulate(windowTable.begin(), windowTable.end(), 0.0f);
        const float fftNormalize = 1.0f / static_cast<float>(FFT_SIZE_HALF);
        zlth::simd::multiply_inplace(windowTable, windowNormalize * fftNormalize);

        for (int i = 0; i < 32; ++i)
        {
            auto& data = pathFifo.getBufferAt(i);
            data.spectrumPath.assign(FFT_SIZE_HALF, -100.0f);
            data.peakHoldPath.assign(FFT_SIZE_HALF, -100.0f);
        }
    }
    void process(double sampleRate)
    {
        juce::AudioBuffer<float> incomingBufferL, incomingBufferR;
        while (channelFifoL->getNumCompleteBuffersAvailable() > 0 && channelFifoR->getNumCompleteBuffersAvailable() > 0)
        {
            if (channelFifoL->getAudioBuffer(incomingBufferL) && channelFifoR->getAudioBuffer(incomingBufferR))
            {
                const int originalIncomingSize = incomingBufferL.getNumSamples();
                const float deltaTime = originalIncomingSize / sampleRate;
                decibelLCurrent = juce::Decibels::gainToDecibels(incomingBufferL.getMagnitude(0, 0, originalIncomingSize));
                decibelRCurrent = juce::Decibels::gainToDecibels(incomingBufferR.getMagnitude(0, 0, originalIncomingSize));
                const int useSize = std::min(originalIncomingSize, FFT_SIZE);
                const int sourceOffset = originalIncomingSize - useSize;
                const int copySize = FFT_SIZE - useSize;
                if (copySize > 0)
                {
                    std::memmove(audioBuffer.data(), audioBuffer.data() + useSize, copySize * sizeof(float));
                }
                std::copy(incomingBufferL.getReadPointer(0, sourceOffset), incomingBufferL.getReadPointer(0, sourceOffset) + useSize, audioBuffer.begin() + copySize);

                std::copy(audioBuffer.begin(), audioBuffer.end(), fftBufferReal.begin());
                fftBufferImag.fill(0.0f);

                zlth::simd::multiply_two_buffers(fftBufferReal, windowTable);
                fft.performFFT(fftBufferReal, fftBufferImag);
                auto realPart = std::span(fftBufferReal).first(FFT_SIZE_HALF);
                auto imagPart = std::span(fftBufferImag).first(FFT_SIZE_HALF);
                zlth::simd::complex_power(powersBuffer, realPart, imagPart);
                const float factor = 1.0f - std::exp(-deltaTime * 50.0f);
                for (size_t i = 0; i < FFT_SIZE_HALF; ++i)
                {
                    const float target = powersBuffer[i];
                    float released = powers[i] + factor * (target - powers[i]);
                    powers[i] = std::max(target, released);
                    powersBuffer[i] = powers[i];
                    decibelsCurrent[i] = std::log10(std::max(1e-10f, powersBuffer[i])) * 10.0f;
                }

                const float peakFall = 15.0f * deltaTime;
                const float meterFall = 50.0f * deltaTime;
                zlth::simd::apply_falloff(decibelsPeak, decibelsCurrent, peakFall);
                decibelLSmoothed = juce::jmax(decibelLCurrent, decibelLSmoothed - meterFall);
                decibelRSmoothed = juce::jmax(decibelRCurrent, decibelRSmoothed - meterFall);
            }
        }
        if (auto* renderData = pathFifo.getWriteBuffer())
        {
            std::copy(decibelsCurrent.begin(), decibelsCurrent.end(), renderData->spectrumPath.begin());
            std::copy(decibelsPeak.begin(), decibelsPeak.end(), renderData->peakHoldPath.begin());
            renderData->dbM = decibelLSmoothed;
            renderData->dbS = decibelRSmoothed;
            pathFifo.finishedWrite();
        }
    }
    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }
    bool getPath(SpectrumRenderData& path)
    {
        if (auto* renderData = pathFifo.getReadBuffer())
        {
            path.spectrumPath = renderData->spectrumPath;
            path.peakHoldPath = renderData->peakHoldPath;
            path.dbM = renderData->dbM;
            path.dbS = renderData->dbS;

            pathFifo.finishedRead();
            return true;
        }
        return false;
    }
    std::vector<float> makeFreqLUT(const double sampleRate, const float minHz, const float maxHz) const
    {
        std::vector<float> frequencyLUT;
        frequencyLUT.reserve(FFT_SIZE_HALF);
        const float binWidth = static_cast<float>(sampleRate / FFT_SIZE);
        for (int i = 0; i < FFT_SIZE_HALF; ++i)
        {
            frequencyLUT.push_back(juce::mapFromLog10((binWidth * i), minHz, maxHz));
        }
        return frequencyLUT;
    }
private:
    static constexpr int FFT_ORDER = 12;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int FFT_SIZE_HALF = FFT_SIZE >> 1;
    std::array<float, FFT_SIZE> fftBufferReal {};
    std::array<float, FFT_SIZE> fftBufferImag {};
    std::array<float, FFT_SIZE_HALF> powersBuffer {};
    std::array<float, FFT_SIZE> audioBuffer {};
    std::array<float, FFT_SIZE> windowTable {};
    std::array<float, FFT_SIZE_HALF> powers {};
    zlth::dsp::fft::Radix4<FFT_ORDER> fft;

    SingleChannelSampleFifo* channelFifoL;
    SingleChannelSampleFifo* channelFifoR;
    std::vector<float> decibelsCurrent;
    std::vector<float> decibelsPeak;
    float decibelLCurrent = 0.0f;
    float decibelRCurrent = 0.0f;
    float decibelLSmoothed = -100.0f;
    float decibelRSmoothed = -100.0f;
    Fifo<SpectrumRenderData> pathFifo;
};
