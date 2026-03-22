#pragma once

#include <cmath>
#include <numbers>

namespace zlth::dsp::filter
{
    class ZdfSvf2ndOrder
    {
    public:
        ZdfSvf2ndOrder() = default;
        ~ZdfSvf2ndOrder() = default;
        enum class Type
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
        void update_coefficients(Type type, double freqHz, double Q, double dbGain, double sampleRate)
        {
            double freqSafe = std::min(freqHz, sampleRate * 0.49);
            double sqrtA = std::pow(10.0, dbGain / 40.0);
            double A = sqrtA * sqrtA;
            double g = std::tan(std::numbers::pi_v<double> *freqSafe / sampleRate);
            double k = 1.0 / std::max(Q, 0.01);
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
                k /= sqrtA;
                m0 = 1.0;
                m1 = k * (A - 1.0);
                m2 = 0.0;
                break;
            case Type::LowShelf:
                g /= std::sqrt(sqrtA);
                m0 = 1.0;
                m1 = k * (sqrtA - 1.0);
                m2 = A - 1.0;
                break;
            case Type::HighShelf:
                g *= std::sqrt(sqrtA);
                m0 = A;
                m1 = k * (sqrtA - A);
                m2 = 1.0 - A;
                break;
            case Type::Tilt:
                g *= sqrtA;
                m0 = A;
                m1 = k * (1.0 - A);
                m2 = 1.0 / A - A;
                break;
            case Type::Notch:
                m0 = 1.0;
                m1 = -k;
                m2 = 0.0;
                break;
            case Type::BandPass:
                m0 = 0.0;
                m1 = k;
                m2 = 0.0;
                break;
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
            double w2 = w * w;
            double den = (1.0 - w2) * (1.0 - w2) + (currentK * currentK * w2);
            double real = m0 * (1.0 - w2) + m2;
            double imag = (m0 * currentK + m1) * w;
            return std::sqrt((real * real + imag * imag) / den);
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
    class ZdfSvf1stOrder
    {
    public:
        ZdfSvf1stOrder() = default;
        ~ZdfSvf1stOrder() = default;
        enum class Type
        {
            LowPass = 0,
            HighPass,
            LowShelf,
            HighShelf
        };
        void reset() noexcept
        {
            s1 = 0.0;
        }
        void update_coefficients(Type type, double freqHz, double dbGain, double sampleRate)
        {
            double freqSafe = std::min(freqHz, sampleRate * 0.49);
            double g = std::tan(std::numbers::pi_v<double> *freqSafe / sampleRate);
            double A = std::pow(10.0, dbGain / 20.0);
            G = g / (1.0 + g);
            switch (type)
            {
            case Type::LowPass:
                m0 = 0.0;
                m1 = 1.0;
                break;
            case Type::HighPass:
                m0 = 1.0;
                m1 = -1.0;
                break;
            case Type::LowShelf:
                m0 = 1.0;
                m1 = A - 1.0;
                break;
            case Type::HighShelf:
                m0 = A;
                m1 = 1.0 - A;
                break;
            }
            currentG = g;
            currentA = A;
        }
        double get_magnitude(const double freqHz, const double sampleRate) const
        {
            double w = std::tan(std::numbers::pi_v<double> *freqHz / sampleRate) / currentG;
            double den = std::sqrt(1.0 + w * w);
            double real = (m0 + m1);
            double imag = m0 * w;
            return std::sqrt(real * real + imag * imag) / den;
        }
        double process_sample(const double v0) noexcept
        {
            double v1 = (v0 - s1) * G + s1;
            double y = m0 * v0 + m1 * v1;
            s1 = 2.0 * v1 - s1;
            return y;
        }
    private:
        double G {0.0};
        double m0 {0.0}, m1 {0.0};
        double s1 {0.0};
        double currentG {0.0};
        double currentA {1.0};
    };
}
