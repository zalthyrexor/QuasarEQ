#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zlth::dsp::filter
{
    static constexpr double pi = std::numbers::pi_v<double>;
    static constexpr double ln10_div_40 = std::numbers::ln10_v<double> / 40.0;

    class ZdfSvf2ndOrder
    {
    public:
        ZdfSvf2ndOrder() = default;
        ~ZdfSvf2ndOrder() = default;
        void reset() noexcept
        {
            ic1eq = 0.0;
            ic2eq = 0.0;
        }
        double process_sample(const double v0) noexcept
        {
            double v3 = v0 - ic2eq;
            double v1 = a1 * ic1eq + a2 * v3;
            double v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0 * v1 - ic1eq;
            ic2eq = 2.0 * v2 - ic2eq;
            return m0 * v0 + m1 * v1 + m2 * v2;
        }
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
        void update_coefficients(FilterType filterType, double freqHz, double q, double dbGain, double sampleRate)
        {
            double safeFreq = std::clamp(freqHz, sampleRate * 0.0001, sampleRate * 0.49);
            double safeQ = std::max(q, 0.01);
            double g = std::tan(pi * safeFreq / sampleRate);
            double k = 1.0 / safeQ;
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
                double sqrtA = std::exp(ln10_div_40 * dbGain);
                double A = sqrtA * sqrtA;
                k /= sqrtA;
                m0 = 1.0;
                m1 = k * (A - 1.0);
                m2 = 0.0;
                break;
            }
            case FilterType::LowShelf:
            {
                double sqrtA = std::exp(ln10_div_40 * dbGain);
                double A = sqrtA * sqrtA;
                g /= std::sqrt(sqrtA);
                m0 = 1.0;
                m1 = k * (sqrtA - 1.0);
                m2 = A - 1.0;
                break;
            }
            case FilterType::HighShelf:
            {
                double sqrtA = std::exp(ln10_div_40 * dbGain);
                double A = sqrtA * sqrtA;
                g *= std::sqrt(sqrtA);
                m0 = A;
                m1 = k * (sqrtA - A);
                m2 = 1.0 - A;
                break;
            }
            case FilterType::Tilt:
            {
                double sqrtA = std::exp(ln10_div_40 * dbGain);
                double A = sqrtA * sqrtA;
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
            a2 = g * a1;
            a3 = g * a2;
            currentG = g;
            currentK = k;
        }
        double get_magnitude(const double freqHz, const double sampleRate) const
        {
            double safeFreq = std::clamp(freqHz, sampleRate * 0.0001, sampleRate * 0.49);
            double w = std::tan(pi * safeFreq / sampleRate) / currentG;
            double kw = currentK * w;
            double t = 1.0 - w * w;
            double real = m0 * t + m2;
            double imag = (m0 * currentK + m1) * w;
            return std::sqrt((real * real + imag * imag) / (t * t + kw * kw));
        }
    private:
        double a1 {0.0}, a2 {0.0}, a3 {0.0};
        double m0 {1.0}, m1 {0.0}, m2 {0.0};
        double ic1eq {0.0};
        double ic2eq {0.0};
        double currentG {1.0};
        double currentK {1.0};
    };

    class ZdfSvf1stOrder
    {
    public:
        ZdfSvf1stOrder() = default;
        ~ZdfSvf1stOrder() = default;
        enum class FilterType
        {
            HighPass,
            LowPass,
            HighShelf,
            LowShelf,
            Tilt
        };
    };
}

// A     = 10^(dB/20) = exp(ln(10)/20 * dB)
// sqrtA = 10^(dB/40) = exp(ln(10)/40 * dB)
// v0 Input
// v1 1st integrator output (Band-Pass behavior)
// v2 2nd integrator output (Low-Pass behavior)
// m0, m1, m2: Mixing weights for v0, v1, v2
// Overwrite g and/or k in-place to ensure symmetrical boost/cut.
// Response warping near Nyquist is expected behavior.

// https://gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b
// http://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
