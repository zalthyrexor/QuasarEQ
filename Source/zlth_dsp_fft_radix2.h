#pragma once

#include <vector>
#include <cmath>
#include <numbers>
#include <span>

namespace zlth::dsp::fft::radix2
{
    template <size_t Order>
    class Radix2 
    {
    public:
        static constexpr size_t N = 1ULL << Order;
        Radix2() 
        {
            for (size_t i = 0; i < N; ++i) 
            {
                size_t rev = 0;
                for (size_t j = 0; j < Order; ++j) 
                {
                    if ((i >> j) & 1) rev |= (1ULL << (Order - 1 - j));
                }
                bitRevTable[i] = rev;
            }
            for (size_t i = 0; i < N / 2; ++i) 
            {
                float theta = -2.0f * std::numbers::pi_v<float> *i / N;
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
            for (size_t s = 1; s <= Order; ++s)
            {
                const size_t m = 1ULL << s;
                const size_t half = m >> 1;
                const size_t step = N / m;
                for (size_t j = 0; j < half; ++j)
                {
                    const float wr = twiddleR[j * step];
                    const float wi = twiddleI[j * step];
                    for (size_t k = 0; k < N; k += m)
                    {
                        const size_t i0 = k + j;
                        const size_t i1 = k + j + half;
                        const float tr = real[i1] * wr - imag[i1] * wi;
                        const float ti = real[i1] * wi + imag[i1] * wr;
                        real[i1] = real[i0] - tr;
                        imag[i1] = imag[i0] - ti;
                        real[i0] = real[i0] + tr;
                        imag[i0] = imag[i0] + ti;
                    }
                }
            }
        }
    private:
        std::array<size_t, N> bitRevTable;
        std::array<float, N / 2> twiddleR;
        std::array<float, N / 2> twiddleI;
    };
}
