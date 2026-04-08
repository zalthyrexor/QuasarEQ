#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include "zlth_dsp_filter_tpt1pole.h"
#include "zlth_dsp_filter_tpt2pole.h"
/*
namespace zlth::dsp::filter
{
    template <size_t Stages>
    class Cascade
    {
    public:
        void set_params(float freqHz, float sampleRate)
        {
            for (size_t i = 0; i < Stages; ++i)
            {
                float k = 2.0f * std::cos((2.0f * i + 1.0f) * pi / (4.0f * Stages));
                stages[i].set_coefficients(TPT2Pole::FilterType::LowPass, freqHz, k, 0.0f, sampleRate);
            }
        }
        float process(float input) noexcept
        {
            float output = input;
            for (auto& stage : stages)
            {
                output = stage.process_sample(output);
            }
            return output;
        }
    private:
        std::array<TPT2Pole, Stages> stages;
        static constexpr float pi = std::numbers::pi_v<float>;
    };
}
*/
