#pragma once

#include <cmath>
#include <numbers>

namespace zlth::dsp::filter
{
    class ZdfSvfFilter
    {
    public:
        ZdfSvfFilter() = default;
        ~ZdfSvfFilter() = default;
        enum class Type
        {
            HighPass = 0,
            HighShelf,
            LowPass,
            LowShelf,
            Peak,
            Tilt
        };
        void reset()
        {
            ic1eq = 0.0;
            ic2eq = 0.0;
        }
        void update_coefficients(Type type, double freqHz, double Q, double dbGain, double sampleRate)
        {
            double freqSafe = std::min(freqHz, sampleRate * 0.49);
            double A = std::pow(10.0, dbGain / 40.0);
            double g = std::tan(std::numbers::pi_v<double> *freqSafe / sampleRate);
            double k = 1.0 / Q;
            switch (type)
            {
            case Type::LowPass:
                m0 = 0.0;
                m1 = 0.0;
                m2 = 1.0;
                break;
            case Type::HighPass:
                m0 = 1.0;
                m1 = -k;
                m2 = -1.0;
                break;
            case Type::Peak:
                k /= A;
                m0 = 1.0;
                m1 = k * (A * A - 1.0);
                m2 = 0.0;
                break;
            case Type::LowShelf:
                g /= std::sqrt(A);
                m0 = 1.0;
                m1 = k * (A - 1.0);
                m2 = (A * A - 1.0);
                break;
            case Type::HighShelf:
                g *= std::sqrt(A);
                m0 = A * A;
                m1 = k * (1.0 - A) * A;
                m2 = 1.0 - A * A;
                break;
            case Type::Tilt:
                g *= std::sqrt(A);
                m0 = A;
                m1 = k * (1.0 - A);
                m2 = 1.0 / A - A;
                break;
            }
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
            currentG = g;
            currentK = k;
        }
        float get_magnitude(const float freqHz, const float sampleRate) const
        {
            const float pi = std::numbers::pi_v<float>;
            const float w = std::tan(pi * freqHz / sampleRate) / currentG;
            const float w2 = w * w;
            const float den = (1.0f - w2) * (1.0f - w2) + (currentK * currentK * w2);
            const float real = m0 * (1.0f - w2) + m2;
            const float imag = (m0 * currentK + m1) * w;
            return std::sqrt((real * real + imag * imag) / den);
        }
        float process_sample(const float input) noexcept
        {
            double v0 = static_cast<double>(input);
            double v3 = v0 - ic2eq;
            double v1 = a1 * ic1eq + a2 * v3;
            double v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0 * v1 - ic1eq;
            ic2eq = 2.0 * v2 - ic2eq;
            double output = m0 * v0 + m1 * v1 + m2 * v2;
            return static_cast<float>(output);
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
