#pragma once

#include <vector>
#include "zlth_fifo.h"
#include "zlth_dsp_fft_radix4.h"
#include "zlth_simd.h"
#include "unit.h"

struct SpectrumRenderData
{
    std::vector<float> spectrumDb;
    std::vector<float> spectrumPeakDb;
    std::array<float, 2> meterLevelsDb {-100.0f, -100.0f};
    std::array<float, 2> meterLevelsPeakDb {-100.0f, -100.0f};
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
        zlth::simd::mul_inplace(windowTable_mul_fftNormalize, 1.0f / static_cast<float>(FFT_SIZE_HALF));
        for (int i = 0; i < 32; ++i) {
            auto& data = pathFifo.getBufferAt(i);
            data.spectrumDb.assign(FFT_SIZE_HALF, -100.0f);
            data.spectrumPeakDb.assign(FFT_SIZE_HALF, -100.0f);
        }
    }
    void process(double sampleRate) {
        std::array< std::vector<float>, 2> buffer {};
        while (fifo[0].getNumAvailable() > 0 && fifo[1].getNumAvailable() > 0) {
            if (!fifo[0].pull(buffer[0]) || !fifo[1].pull(buffer[1])) {
                continue;
            }
            const int originalIncomingSize = buffer[0].size();
            const float deltaTime = originalIncomingSize / sampleRate;
            const float spectrumSmoothing = 1.0f - std::exp(-deltaTime * 50.0f);
            const float peakFallRate = 15.0f * deltaTime;
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
            zlth::simd::magnitude_sqr(currentMagnitudes, fftReal, fftImag);
            zlth::simd::lerp_inplace(smoothedMagnitudes, currentMagnitudes, spectrumSmoothing);
            zlth::simd::max_inplace(smoothedMagnitudes, currentMagnitudes);
            zlth::simd::max_inplace(smoothedMagnitudes, 1e-10f);
            zlth::simd::mag_sq_to_db(decibelsCurrent, smoothedMagnitudes);
            zlth::simd::sub_inplace(decibelsPeak, peakFallRate);
            zlth::simd::max_inplace(decibelsPeak, decibelsCurrent);
            const float meterFall = 1.0f - std::exp(-deltaTime * 10.0f);
            const float meterFallRate = 6.0f * deltaTime;
            for (int i = 0; i < 2; ++i) {
                currentPeakLinear[i] = zlth::simd::get_abs_max(buffer[i]);
                smoothedPeakLinear[i] += meterFall * (currentPeakLinear[i] - smoothedPeakLinear[i]);
                smoothedPeakLinear[i] = juce::jmax(currentPeakLinear[i], smoothedPeakLinear[i]);
                meterLevelsPeakDb[i] -= meterFallRate;
                meterLevelsPeakDb[i] = std::max(meterLevelsPeakDb[i], zlth::unit::magToDB(smoothedPeakLinear[i]));
            }
        }
        if (auto* renderData = pathFifo.getWriteBuffer()) {
            std::copy(decibelsCurrent.begin(), decibelsCurrent.end(), renderData->spectrumDb.begin());
            std::copy(decibelsPeak.begin(), decibelsPeak.end(), renderData->spectrumPeakDb.begin());
            for (int i = 0; i < 2; ++i) {
                renderData->meterLevelsDb[i] = zlth::unit::magToDB(smoothedPeakLinear[i]);
                renderData->meterLevelsPeakDb[i] = meterLevelsPeakDb[i];
            }
            pathFifo.finishedWrite();
        }
    }
    int getNumPathsAvailable() const {
        return pathFifo.getNumAvailableForReading();
    }
    bool getPath(SpectrumRenderData& path) {
        auto* renderData = pathFifo.getReadBuffer();
        if (renderData == nullptr) {
            return false;
        }
        path.spectrumDb = renderData->spectrumDb;
        path.spectrumPeakDb = renderData->spectrumPeakDb;
        for (int i = 0; i < 2; ++i) {
            path.meterLevelsDb[i] = renderData->meterLevelsDb[i];
            path.meterLevelsPeakDb[i] = renderData->meterLevelsPeakDb[i];
        }
        pathFifo.finishedRead();
        return true;
    }
private:
    static void fill_blackman_harris(std::span<float> target) noexcept {
        auto size = target.size();
        float invSize = std::numbers::pi_v<float> *2.0f / static_cast<float>(size);
        for (std::size_t i = 0; i < size; ++i) {
            float phi = static_cast<float>(i) * invSize;
            target[i] = 0.35875f - 0.48829f * std::cos(phi) + 0.14128f * std::cos(2.0f * phi) - 0.01168f * std::cos(3.0f * phi);
        }
    }
    static constexpr int FFT_ORDER {12};
    static constexpr int FFT_SIZE {1 << FFT_ORDER};
    static constexpr int FFT_SIZE_HALF {FFT_SIZE >> 1};
    std::array<float, FFT_SIZE> fftReal {};
    std::array<float, FFT_SIZE> fftImag {};
    std::array<float, FFT_SIZE> audioBuffer {};
    std::array<float, FFT_SIZE> windowTable_mul_fftNormalize {};
    std::array<float, FFT_SIZE_HALF> currentMagnitudes {};
    std::array<float, FFT_SIZE_HALF> smoothedMagnitudes {};
    std::array<float, FFT_SIZE_HALF> decibelsCurrent;
    std::array<float, FFT_SIZE_HALF> decibelsPeak;
    std::array<float, 2> currentPeakLinear {0.0f, 0.0f};
    std::array<float, 2> smoothedPeakLinear {0.0f, 0.0f};
    std::array<float, 2> meterLevelsPeakDb {-100.0f, -100.0f};
    std::array<SampleFifo, 2>& fifo;
    zlth::dsp::fft::Radix4<FFT_ORDER> fft;
    Fifo<SpectrumRenderData> pathFifo;
};
