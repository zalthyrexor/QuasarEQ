#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace zlth::dsp::filter
{
    class TPT1Pole
    {
    public:
        TPT1Pole() = default;
        ~TPT1Pole() = default;
        enum class FilterType { HighPass, LowPass, HighShelf, LowShelf, Tilt };
        void set_coefficients(FilterType filterType, float freqHz, float gainDb, float sampleRate) noexcept
        {
            float preG = calculate_g(freqHz, sampleRate);
            float sqrtA = std::exp(ln10_div_40 * gainDb);
            float A = sqrtA * sqrtA;
            switch (filterType)
            {
            case FilterType::LowPass:
                m0 = 0.0f;
                m1 = 1.0f;
                break;
            case FilterType::HighPass:
                m0 = 1.0f;
                m1 = -1.0f;
                break;
            case FilterType::LowShelf:
                preG /= sqrtA;
                m0 = 1.0f;
                m1 = A - 1.0f;
                break;
            case FilterType::HighShelf:
                preG *= sqrtA;
                m0 = A;
                m1 = 1.0f - A;
                break;
            case FilterType::Tilt:
                preG *= sqrtA;
                m0 = A;
                m1 = 1.0f / A - A;
                break;
            default:
                m0 = 1.0f;
                m1 = 0.0f;
                break;
            }
            g = preG;
            a1 = 1.0f / (1.0f + preG);
        }
        void reset() noexcept
        {
            ic1eq = 0.0f;
        }
        float process_sample(const float v0) noexcept
        {
            float v1 = a1 * (g * v0 + ic1eq);
            ic1eq = 2.0f * v1 - ic1eq;
            return m0 * v0 + m1 * v1;
        }
        std::complex<float> get_response(const float freqHz, const float sampleRate) const noexcept
        {
            std::complex<float> s {0.0f, calculate_g(freqHz, sampleRate) / g};
            return m0 + m1 / (1.0f + s);
        }
    private:
        static float calculate_g(float freqHz, float sampleRate)
        {
            return std::tan(pi * std::clamp(freqHz, sampleRate * freqMin, sampleRate * freqMax) / sampleRate);
        }
        float g {};
        float a1 {};
        float ic1eq {0.0f};
        float m0 {}, m1 {};
        static constexpr float freqMin {0.0001f};
        static constexpr float freqMax {0.4999f};
        static constexpr float pi {std::numbers::pi_v<float>};
        static constexpr float ln10_div_40 {std::numbers::ln10_v<float> / 40.0f};
    };
}
