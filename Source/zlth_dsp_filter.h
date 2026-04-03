#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace zlth::dsp::filter
{
    static constexpr float pi = std::numbers::pi_v<float>;
    static constexpr float ln10_div_40 = std::numbers::ln10_v<float> / 40.0f;
    class ZdfSvf2ndOrder
    {
    public:
        ZdfSvf2ndOrder() = default;
        ~ZdfSvf2ndOrder() = default;
        enum class FilterType { HighPass, LowPass, HighShelf, LowShelf, Bell, Tilt, Notch, BandPass };
        void set_coefficients(FilterType filterType, float cutoffFreqHz, float q, float gainDb, float sampleRate)
        {
            float g = std::tan(pi * std::clamp(cutoffFreqHz, sampleRate * 0.0001f, sampleRate * 0.49f) / sampleRate);
            float k = 1.0f / std::max(q, 0.01f);
            float sqrtA = std::exp(ln10_div_40 * gainDb);
            float A = sqrtA * sqrtA;
            switch (filterType)
            {
            case FilterType::LowPass:
                m0 = 0.0f;
                m1 = 0.0f;
                m2 = 1.0f;
                break;
            case FilterType::HighPass:
                m0 = 1.0f;
                m1 = -k;
                m2 = -1.0f;
                break;
            case FilterType::Notch:
                m0 = 1.0f;
                m1 = -k;
                m2 = 0.0f;
                break;
            case FilterType::BandPass:
                m0 = 0.0f;
                m1 = k;
                m2 = 0.0f;
                break;
            case FilterType::Bell:
                k /= sqrtA;
                m0 = 1.0f;
                m1 = k * (A - 1.0f);
                m2 = 0.0f;
                break;
            case FilterType::LowShelf:
                g /= std::sqrt(sqrtA);
                m0 = 1.0f;
                m1 = k * (sqrtA - 1.0f);
                m2 = A - 1.0f;
                break;
            case FilterType::HighShelf:
                g *= std::sqrt(sqrtA);
                m0 = A;
                m1 = k * (sqrtA - A);
                m2 = 1.0f - A;
                break;
            case FilterType::Tilt:
                g *= sqrtA;
                m0 = A;
                m1 = k * (1.0f - A);
                m2 = 1.0f / A - A;
                break;
            default:
                m0 = 1.0f;
                m1 = 0.0f;
                m2 = 0.0f;
                break;
            }
            a1 = 1.0f / (1.0f + g * (g + k));
            currentG = g;
            currentK = k;
        }
        void reset() noexcept
        {
            ic1eq = 0.0f;
            ic2eq = 0.0f;
        }
        float process_sample(const float v0) noexcept
        {
            float v1 = a1 * (ic1eq + currentG * (v0 - ic2eq));
            float v2 = ic2eq + currentG * v1;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            return m0 * v0 + m1 * v1 + m2 * v2;
        }
        std::complex<float> get_response(const float freqHz, const float sampleRate) const
        {
            std::complex<float> s {0.0f, std::tan(pi * std::clamp(freqHz, sampleRate * 0.0001f, sampleRate * 0.49f) / sampleRate) / currentG};
            return m0 + (m1 * s + m2) / (s * s + currentK * s + 1.0f);
        }
    private:
        float m0 {1.0f}, m1 {0.0f}, m2 {0.0f};
        float a1 {0.0f};
        float ic1eq {0.0f};
        float ic2eq {0.0f};
        float currentG {1.0f};
        float currentK {1.0f};
    };
}

// Special thanks to Andrew Simper for the ZDF SVF algorithm.

// Filter coefficients [m0, m1, m2] act as mixing weights for the basis signals:
// [input (v0), band-pass (v1), low-pass (v2)].
// Due to the linearity of the SVF, summing the coefficients of different filter 
// types yields the combined response (e.g., Notch [1, -k, 0] = HP [1, -k, -1] + LP [0, 0, 1]).

// Overwrite g and/or k in-place to ensure symmetrical boost/cut.
// Response warping near Nyquist is expected behavior.
// Implementation is kept in the header to facilitate compiler inlining

// https://gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b
// http://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

// =========================================================================================================
// DISCLAIMER: THIS CODE IS PROVIDED 'AS IS', WITHOUT ANY EXPRESS OR IMPLIED WARRANTY. USE AT YOUR OWN RISK.
// =========================================================================================================
