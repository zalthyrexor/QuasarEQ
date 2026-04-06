#pragma once

#include <vector>
#include "zlth_fifo.h"
#include "zlth_dsp_fft_radix4.h"
#include "zlth_dsp_window.h"
#include "zlth_dsp_window_coefficients.h"
#include "zlth_simd.h"
#include "Ballistics.h"

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
        zlth::simd::multiply(windowTable, windowTable.size() / std::accumulate(windowTable.begin(), windowTable.end(), 0.0));

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
                    std::memmove(audioBuffer.data(), audioBuffer.data() + useSize, copySize * sizeof(float));
                }
                std::copy(incomingBufferL.getReadPointer(0, sourceOffset), incomingBufferL.getReadPointer(0, sourceOffset) + useSize, audioBuffer.begin() + copySize);
                std::copy(audioBuffer.begin(), audioBuffer.end(), fftBufferReal.begin());
                zlth::simd::multiply_two_buffers(fftBufferReal, windowTable);

                fftBufferImag.fill(0.0f);
                fft.performFFT(fftBufferReal, fftBufferImag);

                __m256 v_inv_sqr = _mm256_set1_ps(INVERSE_FFT_SIZE_HALF * INVERSE_FFT_SIZE_HALF);
                for (size_t i = 0; i < FFT_SIZE_HALF; i += 8)
                {
                    __m256 v_r = _mm256_loadu_ps(&fftBufferReal[i]);
                    __m256 v_im = _mm256_loadu_ps(&fftBufferImag[i]);
                    __m256 v_r2 = _mm256_mul_ps(v_r, v_r);
                    __m256 v_im2 = _mm256_mul_ps(v_im, v_im);
                    __m256 v_sum = _mm256_add_ps(v_r2, v_im2);
                    __m256 v_res = _mm256_mul_ps(v_sum, v_inv_sqr);
                    _mm256_storeu_ps(&magnitudesBuffer[i], v_res);
                }

                const float deltaTime = static_cast<float>(originalIncomingSize) / sampleRate;
                spectrumBallistics.process(magnitudesBuffer, deltaTime);
                for (size_t i = 0; i < FFT_SIZE_HALF; ++i)
                {
                    decibelsCurrent[i] = std::log10(std::max(1e-10f, magnitudesBuffer[i])) * 10.0f;
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
    static constexpr float INVERSE_FFT_SIZE_HALF = 1.0f / FFT_SIZE_HALF;

    zlth::dsp::fft::Radix4<FFT_ORDER> fft;

    std::array<float, FFT_SIZE> fftBufferReal {};
    std::array<float, FFT_SIZE> fftBufferImag {};
    std::array<float, FFT_SIZE> audioBuffer {};
    std::array<float, FFT_SIZE> windowTable {};
    std::array<float, FFT_SIZE_HALF> magnitudesBuffer {};

    SingleChannelSampleFifo* channelFifoL;
    SingleChannelSampleFifo* channelFifoR;
    std::vector<float> decibelsCurrent;
    std::vector<float> decibelsPeak;
    float decibelLCurrent = 0.0f;
    float decibelRCurrent = 0.0f;
    float decibelLSmoothed = -100.0f;
    float decibelRSmoothed = -100.0f;
    Fifo<SpectrumRenderData> pathFifo;
    Ballistics<FFT_SIZE_HALF> spectrumBallistics;
};
