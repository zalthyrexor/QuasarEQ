#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace zlth::dsp::filter
{
    using FloatType = float;
    static constexpr FloatType pi = std::numbers::pi_v<FloatType>;
    static constexpr FloatType ln10_div_40 = std::numbers::ln10_v<FloatType> / 40.0;
    class ZdfSvf2ndOrder
    {
    public:
        ZdfSvf2ndOrder() = default;
        ~ZdfSvf2ndOrder() = default;
        enum class FilterType
        {
            HighPass,
            LowPass,
            HighShelf,
            LowShelf,
            Bell,
            Tilt,
            Notch,
            BandPass
        };
        void set_coefficients(FilterType filterType, FloatType freqHz, FloatType q, FloatType dbGain, FloatType sampleRate)
        {
            FloatType safeFreq = std::clamp(freqHz, sampleRate * static_cast<FloatType>(0.0001), sampleRate * static_cast<FloatType>(0.49));
            FloatType safeQ = std::max(q, static_cast<FloatType>(0.01));
            FloatType g = std::tan(pi * safeFreq / sampleRate);
            FloatType k = 1.0 / safeQ;
            switch (filterType)
            {
            case FilterType::LowPass:
            {
                m0 = 0.0;
                m1 = 0.0;
                m2 = 1.0;
                break;
            }
            case FilterType::HighPass:
            {
                m0 = 1.0;
                m1 = -k;
                m2 = -1.0;
                break;
            }
            case FilterType::Notch:
            {
                m0 = 1.0;
                m1 = -k;
                m2 = 0.0;
                break;
            }
            case FilterType::BandPass:
            {
                m0 = 0.0;
                m1 = k;
                m2 = 0.0;
                break;
            }
            case FilterType::Bell:
            {
                FloatType sqrtA = std::exp(ln10_div_40 * dbGain);
                FloatType A = sqrtA * sqrtA;
                k /= sqrtA;
                m0 = 1.0;
                m1 = k * (A - 1.0);
                m2 = 0.0;
                break;
            }
            case FilterType::LowShelf:
            {
                FloatType sqrtA = std::exp(ln10_div_40 * dbGain);
                FloatType A = sqrtA * sqrtA;
                g /= std::sqrt(sqrtA);
                m0 = 1.0;
                m1 = k * (sqrtA - 1.0);
                m2 = A - 1.0;
                break;
            }
            case FilterType::HighShelf:
            {
                FloatType sqrtA = std::exp(ln10_div_40 * dbGain);
                FloatType A = sqrtA * sqrtA;
                g *= std::sqrt(sqrtA);
                m0 = A;
                m1 = k * (sqrtA - A);
                m2 = 1.0 - A;
                break;
            }
            case FilterType::Tilt:
            {
                FloatType sqrtA = std::exp(ln10_div_40 * dbGain);
                FloatType A = sqrtA * sqrtA;
                g *= sqrtA;
                m0 = A;
                m1 = k * (1.0 - A);
                m2 = 1.0 / A - A;
                break;
            }
            default:
            {
                m0 = 1.0;
                m1 = 0.0;
                m2 = 0.0;
            }
            }
            a1 = 1.0 / (1.0 + g * (g + k));
            currentG = g;
            currentK = k;
        }
        void reset() noexcept
        {
            ic1eq = 0.0;
            ic2eq = 0.0;
        }
        FloatType process_sample(const FloatType v0) noexcept
        {
            FloatType v1 = a1 * (ic1eq + currentG * (v0 - ic2eq));
            FloatType v2 = ic2eq + currentG * v1;
            ic1eq = static_cast<FloatType>(2.0) * v1 - ic1eq;
            ic2eq = static_cast<FloatType>(2.0) * v2 - ic2eq;
            return m0 * v0 + m1 * v1 + m2 * v2;
        }
        std::complex<FloatType> get_response(const FloatType freqHz, const FloatType sampleRate) const
        {
            FloatType safeFreq = std::clamp(freqHz, sampleRate * static_cast<FloatType>(0.0001), sampleRate * static_cast<FloatType>(0.49));
            FloatType omega = std::tan(pi * safeFreq / sampleRate) / currentG;
            FloatType denReal = static_cast<FloatType>(1.0) - omega * omega;
            FloatType denImag = currentK * omega;
            FloatType numReal = m0 * denReal + m2;
            FloatType numImag = m0 * denImag + m1 * omega;
            FloatType denNormSq = denReal * denReal + denImag * denImag;
            FloatType resReal = (numReal * denReal + numImag * denImag) / denNormSq;
            FloatType resImag = (numImag * denReal - numReal * denImag) / denNormSq;
            return {resReal, resImag};
        }
    private:
        FloatType m0 {1.0}, m1 {0.0}, m2 {0.0};
        FloatType a1 {0.0};
        FloatType ic1eq {0.0};
        FloatType ic2eq {0.0};
        FloatType currentG {1.0};
        FloatType currentK {1.0};
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
