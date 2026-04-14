#pragma once

#include <vector>
#include "zlth_fifo.h"
#include "zlth_simd.h"
#include "unit.h"

struct SpectrumRenderData {
   std::vector<float> spectrumDb;
   std::vector<float> spectrumPeakDb;
   std::array<float, 2> meterLevelsDb {-100.0f, -100.0f};
   std::array<float, 2> meterLevelsPeakDb {-100.0f, -100.0f};
};

class PathProducer {
public:
   PathProducer(std::array<SampleFifo, 2>& leftScsf): fifo(leftScsf) {
      decibelsPeak.fill(-100.0f);
      decibelsCurrent.fill(0.0f);
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
         const float spectrumSmoothing = 1.0f - std::exp(-deltaTime * 30.0f);
         const float peakFallRate = 15.0f * deltaTime;
         const int useSize = std::min(originalIncomingSize, FFT_SIZE);
         const int sourceOffset = originalIncomingSize - useSize;
         const int copySize = FFT_SIZE - useSize;
         if (copySize > 0) {
            std::memmove(audioBuffer.data(), audioBuffer.data() + useSize, copySize * sizeof(float));
         }
         std::copy(buffer[0].begin() + sourceOffset, buffer[0].end(), audioBuffer.begin() + copySize);
         std::copy(audioBuffer.begin(), audioBuffer.end(), fftReal.begin());
         std::fill(fftReal.begin() + FFT_SIZE, fftReal.end(), 0.0f);

         // 実際に使われるのは FFT_SIZE までだから計算コスト削減（juce::dsp::FFT が FFT_SIZE * 2 を要求する）
         std::span FFTNormalizingSpan {fftReal.data(), FFT_SIZE};

         // fft よりも前にやらないとダメ
         zlth::simd::mul_inplace(FFTNormalizingSpan, FFT_SIZE_HALF_INVERSE);
         windowing.multiplyWithWindowingTable(fftReal.data(), FFT_SIZE);

         // 最後の引数を true にすると後半の不要な部分を計算しないらしい
         fftJuce.performFrequencyOnlyForwardTransform(fftReal.data(), true);

         zlth::simd::lerp_inplace(smoothedMagnitudes, fftReal, spectrumSmoothing);
         zlth::simd::max_inplace(smoothedMagnitudes, fftReal);
         zlth::simd::max_inplace(smoothedMagnitudes, 1e-10f);

         zlth::simd::mag_to_db(decibelsCurrent, smoothedMagnitudes);
         zlth::simd::sub_inplace(decibelsPeak, peakFallRate);
         zlth::simd::max_inplace(decibelsPeak, decibelsCurrent);

         const float meterFall = 1.0f - std::exp(-deltaTime * 10.0f);
         const float meterFallRate = 6.0f * deltaTime;
         for (int i = 0; i < 2; ++i) {
            float temp = zlth::simd::get_abs_max(buffer[i]);
            smoothedPeakLinear[i] += meterFall * (temp - smoothedPeakLinear[i]);
            smoothedPeakLinear[i] = juce::jmax(temp, smoothedPeakLinear[i]);
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
   // 2026 04 14 現在、外部のクラスが 12 Order 以外を受け付けてないから変更しないで
   static constexpr int FFT_ORDER {12};

   // 変数初期化の際に便利
   static constexpr int FFT_SIZE {1 << FFT_ORDER};

   // 実際に描画に用いられる数
   static constexpr int FFT_SIZE_HALF {FFT_SIZE / 2};

   // fft に入るデータを正規化する用（FFT は結果を増幅させる）
   static constexpr float FFT_SIZE_HALF_INVERSE {1.0f / static_cast<float>(FFT_SIZE_HALF)};

   // juce::dsp::FFT が FFT_SIZE * 2 を要求する (FFT_SIZE だと嬉しいんだけどね)
   std::array<float, FFT_SIZE * 2> fftReal {};

   // リングバッファのようなもの
   std::array<float, FFT_SIZE> audioBuffer {};

   // 最後の true で１に正規化
   juce::dsp::WindowingFunction<float> windowing {FFT_SIZE, juce::dsp::WindowingFunction<float>::blackmanHarris, true};

   // FFT の結果を線形補完する用のバッファ
   std::array<float, FFT_SIZE_HALF> smoothedMagnitudes {};

   // 受け取る FIFO
   std::array<SampleFifo, 2>& fifo;

   // 外に渡す FIFO
   Fifo<SpectrumRenderData> pathFifo;

   // FFT ですよ！
   juce::dsp::FFT fftJuce {FFT_ORDER};

   // ↓ --------- 実際にFIFOを介して外に行くデータたち --------- ↓

   // 上書きされるだけの不憫なバッファ
   std::array<float, FFT_SIZE_HALF> decibelsCurrent;

   // レベルメーター用に振幅を線形補完させてる
   std::array<float, 2> smoothedPeakLinear {0.0f, 0.0f};

   // 線形に減衰していく
   std::array<float, FFT_SIZE_HALF> decibelsPeak;
   std::array<float, 2> meterLevelsPeakDb {-100.0f, -100.0f};
};
