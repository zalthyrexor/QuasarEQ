#pragma once
#include <cmath>
#include <array>
#include <immintrin.h>
#include <span>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace zlth
{
    template <size_t Order>
    class alignas(64) FFT
    {
        static_assert(Order % 2 == 0, "Radix-4 requires even Order (e.g., 10, 12).");
    public:
        static constexpr size_t stages = Order;
        static constexpr size_t N = static_cast<size_t>(1) << stages;
        static constexpr float INVERSE_NUM_BINS = 1.0f / (N / 2);
        FFT()
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
                float theta = -2.0f * M_PI * static_cast<float>(i) / static_cast<float>(N);
                twiddleR[i] = std::cos(theta);
                twiddleI[i] = std::sin(theta);
            }
        }
        std::span<const float, N / 2> performFFT(std::span<const float, N> inputData)
        {
            for (size_t i = 0; i < N; ++i)
            {
                tempR[i] = inputData[bitRevTable[i]];
                tempI[i] = 0.0f;
            }
            for (size_t s = 1; s <= stages / 2; ++s)
            {
                const size_t m = static_cast<size_t>(1) << (2 * s);
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
                        float r1 = tempR[i1] * wr1 - tempI[i1] * wi1;
                        float j1 = tempR[i1] * wi1 + tempI[i1] * wr1;
                        float r2 = tempR[i2] * wr2 - tempI[i2] * wi2;
                        float j2 = tempR[i2] * wi2 + tempI[i2] * wr2;
                        float r3 = tempR[i3] * wr3 - tempI[i3] * wi3;
                        float j3 = tempR[i3] * wi3 + tempI[i3] * wr3;
                        float r0_plus_r2 = tempR[i0] + r2;
                        float r0_minus_r2 = tempR[i0] - r2;
                        float r1_plus_r3 = r1 + r3;
                        float r1_minus_r3 = r1 - r3;
                        float j0_plus_j2 = tempI[i0] + j2;
                        float j0_minus_j2 = tempI[i0] - j2;
                        float j1_plus_j3 = j1 + j3;
                        float j1_minus_j3 = j1 - j3;
                        tempR[i0] = r0_plus_r2 + r1_plus_r3;
                        tempI[i0] = j0_plus_j2 + j1_plus_j3;
                        tempR[i1] = r0_minus_r2 + j1_minus_j3;
                        tempI[i1] = j0_minus_j2 - r1_minus_r3;
                        tempR[i2] = r0_plus_r2 - r1_plus_r3;
                        tempI[i2] = j0_plus_j2 - j1_plus_j3;
                        tempR[i3] = r0_minus_r2 - j1_minus_j3;
                        tempI[i3] = j0_minus_j2 + r1_minus_r3;
                    }
                }
            }
            const size_t halfN = N / 2;
            const __m512 vInvN = _mm512_set1_ps(INVERSE_NUM_BINS);
            for (size_t i = 0; i < halfN; i += 16)
            {
                __m512 r = _mm512_load_ps(&tempR[i]);
                __m512 im = _mm512_load_ps(&tempI[i]);
                __m512 magSq = _mm512_add_ps(_mm512_mul_ps(r, r), _mm512_mul_ps(im, im));
                __m512 mag = _mm512_sqrt_ps(magSq);
                _mm512_store_ps(&magnitudes[i], _mm512_mul_ps(mag, vInvN));
            }
            return std::span<const float, N / 2>(magnitudes);
        }
    private:
        alignas(64) std::array<size_t, N> bitRevTable;
        alignas(64) std::array<float, N> twiddleR;
        alignas(64) std::array<float, N> twiddleI;
        alignas(64) std::array<float, N> tempR {};
        alignas(64) std::array<float, N> tempI {};
        alignas(64) std::array<float, N / 2> magnitudes {};
    };
}