#pragma once

#include <vector>
#include "zlth_fifo.h"
#include "zlth_dsp_fft.h"
#include "zlth_dsp_window.h"
#include "zlth_dsp_window_coefficients.h"
#include "zlth_simd.h"
#include "Ballistics.h"

struct SpectrumRenderData
{
    std::vector<float> spectrumPath;
    std::vector<float> peakHoldPath;
    float leftDB = -100.0f;
    float rightDB = -100.0f;
};

class PathProducer
{
public:
    PathProducer(SingleChannelSampleFifo& leftScsf, SingleChannelSampleFifo& rightScsf) : channelFifoL(&leftScsf), channelFifoR(&rightScsf)
    {
        fftBuffer.setSize(1, FFT_SIZE, false, true, true);
        monoBufferL.setSize(1, FFT_SIZE, false, true, true);
        decibelsPeak.assign(FFT_SIZE_HALF, -100.0f);
        windowTable.assign(FFT_SIZE, 0.0f);
        decibelsCurrent.assign(FFT_SIZE_HALF, 0.0f);
        zlth::dsp::window::fill_window(windowTable, zlth::dsp::window::coefficients::blackman_harris_92);
        zlth::simd::multiply(windowTable, windowTable.size() / std::accumulate(windowTable.begin(), windowTable.end(), 0.0));

        magnitudesBuffer.assign(FFT_SIZE_HALF, 0.0f);

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
                decibelLCurrent = juce::Decibels::gainToDecibels(incomingBufferL.getMagnitude(0, 0, originalIncomingSize));
                decibelRCurrent = juce::Decibels::gainToDecibels(incomingBufferR.getMagnitude(0, 0, originalIncomingSize));
                const int useSize = std::min(originalIncomingSize, FFT_SIZE);
                const int sourceOffset = originalIncomingSize - useSize;

                const int copySize = FFT_SIZE - useSize;

                if (copySize > 0)
                {
                    monoBufferL.copyFrom(0, 0, monoBufferL.getReadPointer(0, useSize), copySize);
                }

                monoBufferL.copyFrom(0, copySize, incomingBufferL.getReadPointer(0, sourceOffset), useSize);

                auto destSpan = std::span(fftBuffer.getWritePointer(0), static_cast<size_t>(fftBuffer.getNumSamples()));
                auto midSpan = std::span(monoBufferL.getReadPointer(0), static_cast<size_t>(monoBufferL.getNumSamples()));
                std::copy(midSpan.begin(), midSpan.end(), destSpan.begin());

                const float dt = static_cast<float>(originalIncomingSize) / sampleRate;
                zlth::simd::multiply_two_buffers(destSpan, std::span(windowTable));
                auto magnitudesSpan = std::span(magnitudesBuffer);
                fft.performFFT(destSpan.first<FFT_SIZE>(), magnitudesSpan);
                spectrumBallistics.process(magnitudesSpan, dt);
                zlth::simd::gains_to_decibels(std::span(decibelsCurrent), magnitudesSpan, -100.0f);
                const float peakFall = 15.0f * dt;
                const float meterFall = 50 * dt;
                zlth::simd::apply_falloff(std::span(decibelsPeak), std::span(decibelsCurrent), peakFall);
                decibelLSmoothed = juce::jmax(decibelLCurrent, decibelLSmoothed - meterFall);
                decibelRSmoothed = juce::jmax(decibelRCurrent, decibelRSmoothed - meterFall);
            }
        }
        if (auto* renderData = pathFifo.getWriteBuffer())
        {
            std::copy(decibelsCurrent.begin(), decibelsCurrent.end(), renderData->spectrumPath.begin());
            std::copy(decibelsPeak.begin(), decibelsPeak.end(), renderData->peakHoldPath.begin());
            renderData->leftDB = decibelLSmoothed;
            renderData->rightDB = decibelRSmoothed;
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
            path.leftDB = renderData->leftDB;
            path.rightDB = renderData->rightDB;

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
    SingleChannelSampleFifo* channelFifoL;
    SingleChannelSampleFifo* channelFifoR;
    juce::AudioBuffer<float> monoBufferL;
    juce::AudioBuffer<float> fftBuffer;
    std::vector<float> decibelsCurrent;
    std::vector<float> windowTable;
    zlth::FFT<FFT_ORDER> fft;
    std::vector<float> decibelsPeak;
    float decibelLCurrent = 0.0f;
    float decibelRCurrent = 0.0f;
    float decibelLSmoothed = -100.0f;
    float decibelRSmoothed = -100.0f;
    Fifo<SpectrumRenderData> pathFifo;
    std::vector<float> magnitudesBuffer;
    Ballistics<FFT_SIZE_HALF> spectrumBallistics;
};
