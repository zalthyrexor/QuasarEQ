#pragma once

#include <cmath>
#include <array>
#include <immintrin.h>
#include <span>
#include <complex>
#include <numbers>

namespace zlth::dsp::fft
{
    template <size_t Order>
    class Radix4
    {
        static_assert(Order % 2 == 0, "Radix-4 requires even Order (e.g., 10, 12).");
    public:
        static constexpr size_t stages = Order;
        static constexpr size_t N = static_cast<size_t>(1) << stages;
        Radix4()
        {
            for (size_t i = 0; i < N; ++i)
            {
                size_t rev = 0;
                size_t temp = i;
                for (size_t j = 0; j < stages / 2; ++j)
                {
                    rev = (rev << 2) | (temp & 3);
                    temp >>= 2;
                }
                bitRevTable[i] = rev;
            }
            for (size_t i = 0; i < N; ++i)
            {
                float theta = -2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(N);
                twiddleR[i] = std::cos(theta);
                twiddleI[i] = std::sin(theta);
            }
        }
        void performFFT(std::span<float> real, std::span<float> imag)
        {
            for (size_t i = 0; i < N; ++i)
            {
                size_t j = bitRevTable[i];
                if (i < j)
                {
                    std::swap(real[i], real[j]);
                    std::swap(imag[i], imag[j]);
                }
            }
            for (size_t s = 1; s <= stages / 2; ++s)
            {
                const size_t m = 1ULL << (2 * s);
                const size_t m4 = m >> 2;
                const size_t step = N / m;
                for (size_t j = 0; j < m4; ++j)
                {
                    float wr1 = twiddleR[j * step];
                    float wi1 = twiddleI[j * step];
                    float wr2 = twiddleR[2 * j * step];
                    float wi2 = twiddleI[2 * j * step];
                    float wr3 = twiddleR[3 * j * step];
                    float wi3 = twiddleI[3 * j * step];
                    for (size_t k = j; k < N; k += m)
                    {
                        const size_t i0 = k;
                        const size_t i1 = k + m4;
                        const size_t i2 = k + 2 * m4;
                        const size_t i3 = k + 3 * m4;
                        float r1 = real[i1] * wr1 - imag[i1] * wi1;
                        float j1 = real[i1] * wi1 + imag[i1] * wr1;
                        float r2 = real[i2] * wr2 - imag[i2] * wi2;
                        float j2 = real[i2] * wi2 + imag[i2] * wr2;
                        float r3 = real[i3] * wr3 - imag[i3] * wi3;
                        float j3 = real[i3] * wi3 + imag[i3] * wr3;
                        float r0_plus_r2 = real[i0] + r2;
                        float r0_minus_r2 = real[i0] - r2;
                        float r1_plus_r3 = r1 + r3;
                        float r1_minus_r3 = r1 - r3;
                        float j0_plus_j2 = imag[i0] + j2;
                        float j0_minus_j2 = imag[i0] - j2;
                        float j1_plus_j3 = j1 + j3;
                        float j1_minus_j3 = j1 - j3;
                        real[i0] = r0_plus_r2 + r1_plus_r3;
                        imag[i0] = j0_plus_j2 + j1_plus_j3;
                        real[i1] = r0_minus_r2 + j1_minus_j3;
                        imag[i1] = j0_minus_j2 - r1_minus_r3;
                        real[i2] = r0_plus_r2 - r1_plus_r3;
                        imag[i2] = j0_plus_j2 - j1_plus_j3;
                        real[i3] = r0_minus_r2 - j1_minus_j3;
                        imag[i3] = j0_minus_j2 + r1_minus_r3;
                    }
                }
            }
        }
    private:
        std::array<size_t, N> bitRevTable;
        std::array<float, N> twiddleR;
        std::array<float, N> twiddleI;
    };
}
