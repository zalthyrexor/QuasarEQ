#pragma once

#include <array>
#include "zlth_dsp_filter.h"
#include "config.h"
#include "zlth_simd.h"

template <int BandCount>
class ProcessChain
{
public:
    void process(std::span<float> span) {
        for (int i = 0; i < BandCount; ++i) {
            if (isBandActive[i]) {
                bands[i].process_span(span);
            }
        }
    }
    void set_band_active_state(bool flag, int i) {
        isBandActive[i] = flag;
    }
    void set_band_coefficients(zlth::dsp::Filter::FilterType type, float freq, float qual, float gain, float sampleRate, int i) {
        bands[i].set_coefficients(type, freq, qual, gain, sampleRate);
    }
    std::array<zlth::dsp::Filter, BandCount> bands {};
    std::array<bool, BandCount> isBandActive {};
};
