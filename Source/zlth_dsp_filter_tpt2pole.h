#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <span>

namespace zlth::dsp::filter
{
    class TPT2Pole
    {
    public:
        TPT2Pole() = default;
        ~TPT2Pole() = default;
        enum class FilterType { HighPass, LowPass, HighShelf, LowShelf, Tilt, Bell, Notch, BandPass };
        void set_coefficients(FilterType filterType, float freqHz, float qual, float gainDb, float sampleRate) noexcept {
            float preK = 1.0f / std::max(qual, qualMin);
            float preG = calculate_g(freqHz, sampleRate);
            float sqrtA = std::exp(ln10_div_40 * gainDb);
            float A = sqrtA * sqrtA;
            switch (filterType) {
            case FilterType::LowPass:
                m0 = 0.0f;
                m1 = 0.0f;
                m2 = 1.0f;
                break;
            case FilterType::HighPass:
                m0 = 1.0f;
                m1 = -preK;
                m2 = -1.0f;
                break;
            case FilterType::Notch:
                m0 = 1.0f;
                m1 = -preK;
                m2 = 0.0f;
                break;
            case FilterType::BandPass:
                m0 = 0.0f;
                m1 = preK;
                m2 = 0.0f;
                break;
            case FilterType::Bell:
                preK /= sqrtA;
                m0 = 1.0f;
                m1 = preK * (A - 1.0f);
                m2 = 0.0f;
                break;
            case FilterType::LowShelf:
                preG /= std::sqrt(sqrtA);
                m0 = 1.0f;
                m1 = preK * (sqrtA - 1.0f);
                m2 = A - 1.0f;
                break;
            case FilterType::HighShelf:
                preG *= std::sqrt(sqrtA);
                m0 = A;
                m1 = preK * (sqrtA - A);
                m2 = 1.0f - A;
                break;
            case FilterType::Tilt:
                preG *= sqrtA;
                m0 = A;
                m1 = preK * (1.0f - A);
                m2 = 1.0f / A - A;
                break;
            default:
                m0 = 1.0f;
                m1 = 0.0f;
                m2 = 0.0f;
                break;
            }
            k = preK;
            g = preG;
            a1 = 1.0f / (1.0f + preG * (preG + preK));
        }
        void reset() noexcept {
            ic1eq = 0.0f;
            ic2eq = 0.0f;
        }
        [[msvc::forceinline]] void process_span(std::span<float> data) noexcept {
            for (auto& v0 : data) {
                float v1 = a1 * (ic1eq + g * (v0 - ic2eq));
                float v2 = ic2eq + g * v1;
                ic1eq = 2.0f * v1 - ic1eq;
                ic2eq = 2.0f * v2 - ic2eq;
                v0 = m0 * v0 + m1 * v1 + m2 * v2;
            }
        }
        std::complex<float> get_response(const float freqHz, const float sampleRate) const noexcept {
            std::complex<float> s {0.0f, calculate_g(freqHz, sampleRate) / g};
            return m0 + (m1 * s + m2) / (1.0f + s * (s + k));
        }
    private:
        static float calculate_g(float freqHz, float sampleRate) {
            return std::tan(pi * std::clamp(freqHz, sampleRate * freqMin, sampleRate * freqMax) / sampleRate);
        }
        float k {};
        float g {};
        float a1 {};
        float ic1eq {0.0f};
        float ic2eq {0.0f};
        float m0 {}, m1 {}, m2 {};
        static constexpr float qualMin {0.0001f};
        static constexpr float freqMin {0.0001f};
        static constexpr float freqMax {0.4999f};
        static constexpr float pi {std::numbers::pi_v<float>};
        static constexpr float ln10_div_40 {std::numbers::ln10_v<float> / 40.0f};
    };
}

// Special thanks to Andrew Simper for the ZDF SVF algorithm.
// https://gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b
// http://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
