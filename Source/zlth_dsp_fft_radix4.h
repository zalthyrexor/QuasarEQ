#pragma once

#include <cmath>
#include <array>
#include <immintrin.h>
#include <span>
#include <complex>
#include <numbers>
#include "zlth_dsp_fft_radix4_core.h"

namespace zlth::dsp::fft
{
    template <size_t stages>
    class Radix4
    {
        static_assert(stages % 2 == 0, "Radix-4 requires even Order (e.g., 10, 12).");
    public:
        static constexpr size_t FFT_SIZE = static_cast<size_t>(1) << stages;
        Radix4()
        {
        }
        void performFFT(std::span<float> real, std::span<float> imag)
        {
        }
    private:
    };
}
