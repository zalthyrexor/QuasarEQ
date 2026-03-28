#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zlth::dsp::filter
{
    constexpr double ln10_div_40 = std::numbers::ln10_v<double> / 40.0;
    class ZdfSvf2ndOrder
    {
    public:
        ZdfSvf2ndOrder() = default;
        ~ZdfSvf2ndOrder() = default;
        enum class FilterType
        {
            HighPass = 0,
            HighShelf,
            LowPass,
            LowShelf,
            Peak,
            Tilt,
            Notch,
            BandPass
        };
        void reset() noexcept
        {
            ic1eq = 0.0;
            ic2eq = 0.0;
        }
        void update_coefficients(FilterType filterType, double freqHz, double Q, double dbGain, double sampleRate)
        {
            double safeFreq = std::min(freqHz, sampleRate * 0.49);
            double safeQ = std::max(Q, 0.01);
            double g = std::tan(std::numbers::pi_v<double> *safeFreq / sampleRate);
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
            case FilterType::Peak:
            {
                double sqrtA = std::exp(dbGain * ln10_div_40);
                double A = sqrtA * sqrtA;
                k /= sqrtA;
                m0 = 1.0;
                m1 = k * (A - 1.0);
                m2 = 0.0;
                break;
            }
            case FilterType::LowShelf:
            {
                double sqrtA = std::exp(dbGain * ln10_div_40);
                double A = sqrtA * sqrtA;
                g /= std::sqrt(sqrtA);
                m0 = 1.0;
                m1 = k * (sqrtA - 1.0);
                m2 = A - 1.0;
                break;
            }
            case FilterType::HighShelf:
            {
                double sqrtA = std::exp(dbGain * ln10_div_40);
                double A = sqrtA * sqrtA;
                g *= std::sqrt(sqrtA);
                m0 = A;
                m1 = k * (sqrtA - A);
                m2 = 1.0 - A;
                break;
            }
            case FilterType::Tilt:
            {
                double sqrtA = std::exp(dbGain * ln10_div_40);
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
            double w = std::tan(std::numbers::pi_v<double> *freqHz / sampleRate) / currentG;
            double kw = currentK * w;
            double t = 1.0 - w * w;
            double real = m0 * t + m2;
            double imag = (m0 * currentK + m1) * w;
            return std::sqrt((real * real + imag * imag) / (t * t + kw * kw));
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
    private:
        double a1 {0.0}, a2 {0.0}, a3 {0.0};
        double m0 {0.0}, m1 {0.0}, m2 {0.0};
        double ic1eq {0.0};
        double ic2eq {0.0};
        double currentG {0.0};
        double currentK {1.0};
    };
}

// In this implementation, square root of linear amplitude represents 10^(dB/40).
// m0, m1, m2: mixing coefficients for input, band-pass, and low-pass components.
// Reference:
// https://gist.github.com/hollance/2891d89c57adc71d9560bcf0e1e55c4b
// http://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
