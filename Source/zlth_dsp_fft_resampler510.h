#pragma once

#include <algorithm>
#include <utility>
#include <vector>

namespace zlth::dsp::fft {
   static constexpr int FFT_SIZE = 1 << 12;
   static constexpr int NUM_SECTIONS = 1 << 3;
   static constexpr int SECTION_SIZE = 1 << 8;
   static constexpr int RENDER_OUT_SIZE = 510;
   class Resampler510 {
   public:
      Resampler510() = default;
      ~Resampler510() = default;
      std::vector<std::pair<float, float>> resample(const float* data, const double sampleRate) {
         std::vector<std::pair<float, float>> spectrum;
         spectrum.reserve(RENDER_OUT_SIZE);
         const float binWidth = static_cast<float>(sampleRate / FFT_SIZE);
         int outIndex = 0;
         int dataIndex = 0;
         int levelIndex = 0;
         for (; levelIndex < NUM_SECTIONS; ++levelIndex) {
            const int windowSize = 1 << levelIndex;
            const int nextOutputStart = outIndex + (SECTION_SIZE >> levelIndex);
            for (; outIndex < nextOutputStart; ++outIndex) {
               const float freq = dataIndex * binWidth;
               const float gain = *std::max_element(data + dataIndex, data + dataIndex + windowSize);
               spectrum.emplace_back(freq, gain);
               dataIndex += windowSize;
            }
         }
         return spectrum;
      }
   };
}

// Resampler510
// Designed and implemented by Zalthyrexor.
// 2048-point input to produce a 510-point output.
// The magic number 510 is derived from 256 + 128 + 64 + 32 + 16 + 8 + 4 + 2.
// NOTE: Strictly optimized for FFT_SIZE = 4096. Do not use with other sizes.
