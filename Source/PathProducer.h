#pragma once

#include <vector>
#include "zlth_fifo.h"
#include "zlth_dsp_fft_radix4.h"
#include "zlth_simd.h"

struct SpectrumRenderData
{
    std::vector<float> spectrumPath;
    std::vector<float> peakHoldPath;
    std::array<float, 2> db {-100.0f, -100.0f};
};

class PathProducer
{
public:
    PathProducer(std::array<SampleFifo, 2>& leftScsf): fifo(leftScsf) {
        decibelsPeak.fill(-100.0f);
        decibelsCurrent.fill(0.0f);
        fill_blackman_harris(windowTable_mul_fftNormalize);
        const float windowNormalize = static_cast<float>(windowTable_mul_fftNormalize.size()) / std::accumulate(windowTable_mul_fftNormalize.begin(), windowTable_mul_fftNormalize.end(), 0.0f);
        zlth::simd::mul_inplace(windowTable_mul_fftNormalize, windowNormalize);
        zlth::simd::mul_inplace(windowTable_mul_fftNormalize, fftSizeHalfInverse);
        for (int i = 0; i < 32; ++i) {
            auto& data = pathFifo.getBufferAt(i);
            data.spectrumPath.assign(FFT_SIZE_HALF, -100.0f);
            data.peakHoldPath.assign(FFT_SIZE_HALF, -100.0f);
        }
    }
    void process(double sampleRate) {
        std::array< std::vector<float>, 2> buffer {};
        while (fifo[0].getNumAvailable() > 0 && fifo[1].getNumAvailable() > 0) {
            if (fifo[0].pull(buffer[0]) && fifo[1].pull(buffer[1])) {
                std::span<const float> spanL {buffer[0]};
                std::span<const float> spanR {buffer[1]};
                const int originalIncomingSize = spanL.size();
                const float deltaTime = originalIncomingSize / sampleRate;
                const float powersReleaseFactor = 1.0f - std::exp(-deltaTime * 50.0f);
                const float peaksReleaseFactor = 15.0f * deltaTime;
                decibelLCurrent = juce::Decibels::gainToDecibels(zlth::simd::get_abs_max(spanL));
                decibelRCurrent = juce::Decibels::gainToDecibels(zlth::simd::get_abs_max(spanR));
                const int useSize = std::min(originalIncomingSize, FFT_SIZE);
                const int sourceOffset = originalIncomingSize - useSize;
                const int copySize = FFT_SIZE - useSize;
                if (copySize > 0) {
                    std::memmove(audioBuffer.data(), audioBuffer.data() + useSize, copySize * sizeof(float));
                }
                std::copy(buffer[0].begin() + sourceOffset, buffer[0].end(), audioBuffer.begin() + copySize);
                std::copy(audioBuffer.begin(), audioBuffer.end(), fftReal.begin());
                zlth::simd::mul_inplace(fftReal, windowTable_mul_fftNormalize);
                fftImag.fill(0.0f);
                fft.performFFT(fftReal, fftImag);
                zlth::simd::magnitude_sqr(powersBufferCurrent, fftRealHalf, fftImagHalf);
                zlth::simd::lerp_inplace(powersBuffer, powersBufferCurrent, powersReleaseFactor);
                zlth::simd::max_inplace(powersBuffer, powersBufferCurrent);
                zlth::simd::max_inplace(powersBuffer, 1e-10f);
                zlth::simd::log10(decibelsCurrent, powersBuffer);
                zlth::simd::mul_inplace(decibelsCurrent, 10.0f);
                zlth::simd::sub_inplace(decibelsPeak, peaksReleaseFactor);
                zlth::simd::max_inplace(decibelsPeak, decibelsCurrent);
                const float meterFall = 50.0f * deltaTime;
                decibelLSmoothed = juce::jmax(decibelLCurrent, decibelLSmoothed - meterFall);
                decibelRSmoothed = juce::jmax(decibelRCurrent, decibelRSmoothed - meterFall);
            }
        }
        if (auto* renderData = pathFifo.getWriteBuffer()) {
            std::copy(decibelsCurrent.begin(), decibelsCurrent.end(), renderData->spectrumPath.begin());
            std::copy(decibelsPeak.begin(), decibelsPeak.end(), renderData->peakHoldPath.begin());
            renderData->db[0] = decibelLSmoothed;
            renderData->db[1] = decibelRSmoothed;
            pathFifo.finishedWrite();
        }
    }
    int getNumPathsAvailable() const {
        return pathFifo.getNumAvailableForReading();
    }
    bool getPath(SpectrumRenderData& path) {
        if (auto* renderData = pathFifo.getReadBuffer()) {
            path.spectrumPath = renderData->spectrumPath;
            path.peakHoldPath = renderData->peakHoldPath;
            path.db[0] = renderData->db[0];
            path.db[1] = renderData->db[1];

            pathFifo.finishedRead();
            return true;
        }
        return false;
    }
private:
    static void fill_blackman_harris(std::span<float> target) noexcept {
        auto size = target.size();
        float invSize = std::numbers::pi_v<float> *2.0f / static_cast<float>(size);
        for (std::size_t i = 0; i < size; ++i) {
            float phi = static_cast<float>(i) * invSize;
            target[i] =
                0.35875f -
                0.48829f * std::cos(phi) +
                0.14128f * std::cos(2.0f * phi) -
                0.01168f * std::cos(3.0f * phi);
        }
    }
    static constexpr int FFT_ORDER = 12;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int FFT_SIZE_HALF = FFT_SIZE >> 1;
    const float fftSizeHalfInverse = 1.0f / static_cast<float>(FFT_SIZE_HALF);
    std::array<float, FFT_SIZE> fftReal {};
    std::array<float, FFT_SIZE> fftImag {};
    std::array<float, FFT_SIZE> audioBuffer {};
    std::array<float, FFT_SIZE> windowTable_mul_fftNormalize {};
    std::array<float, FFT_SIZE_HALF> powersBuffer {};
    std::array<float, FFT_SIZE_HALF> powersBufferCurrent {};
    std::span<float, FFT_SIZE_HALF> fftRealHalf {fftReal.data(), FFT_SIZE_HALF};
    std::span<float, FFT_SIZE_HALF> fftImagHalf {fftImag.data(), FFT_SIZE_HALF};
    zlth::dsp::fft::Radix4<FFT_ORDER> fft;
    std::array<SampleFifo, 2>& fifo;
    std::array<float, FFT_SIZE_HALF> decibelsCurrent;
    std::array<float, FFT_SIZE_HALF> decibelsPeak;
    float decibelLCurrent = 0.0f;
    float decibelRCurrent = 0.0f;
    float decibelLSmoothed = -100.0f;
    float decibelRSmoothed = -100.0f;
    Fifo<SpectrumRenderData> pathFifo;
};
