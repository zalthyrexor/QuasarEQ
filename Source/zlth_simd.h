#pragma once

#include <immintrin.h>
#include <ranges>
#include <algorithm>
#include <span>
#include <concepts>
#include <cmath>

namespace zlth::simd
{
    static constexpr int vSize = 256 / (sizeof(float) * 8);
    static void average_two_buffers(std::span<float> dest, std::span<const float> src1, std::span<const float> src2)
    {
        const size_t count = std::min({ dest.size(), src1.size(), src2.size() });
        const __m256 v_half = _mm256_set1_ps(0.5f);
        size_t i = 0;
        for (; i + vSize <= count; i += vSize)
        {
            __m256 s1 = _mm256_loadu_ps(&src1[i]);
            __m256 s2 = _mm256_loadu_ps(&src2[i]);
            _mm256_storeu_ps(&dest[i], _mm256_mul_ps(_mm256_add_ps(s1, s2), v_half));
        }
        for (; i < count; ++i)
        {
            dest[i] = (src1[i] + src2[i]) * 0.5f;
        }
    }
    static void multiply_two_buffers(std::span<float> dest, std::span<const float> src)
    {
        const size_t count = std::min(dest.size(), src.size());
        size_t i = 0;
        for (; i + vSize <= count; i += vSize)
        {
            __m256 d = _mm256_loadu_ps(&dest[i]);
            __m256 s = _mm256_loadu_ps(&src[i]);
            _mm256_storeu_ps(&dest[i], _mm256_mul_ps(d, s));
        }
        for (; i < count; ++i)
        {
            dest[i] *= src[i];
        }
    }
    static void apply_falloff(std::span<float> dest, std::span<const float> src, float fallAmount)
    {
        const size_t count = std::min(dest.size(), src.size());
        const __m256 v_fall = _mm256_set1_ps(fallAmount);
        size_t i = 0;
        for (; i + vSize <= count; i += vSize)
        {
            __m256 d = _mm256_loadu_ps(&dest[i]);
            __m256 s = _mm256_loadu_ps(&src[i]);
            d = _mm256_sub_ps(d, v_fall);
            d = _mm256_max_ps(s, d);
            _mm256_storeu_ps(&dest[i], d);
        }
        for (; i < count; ++i)
        {
            dest[i] = std::max(src[i], dest[i] - fallAmount);
        }
    }
    static void gains_to_decibels(std::span<float> dest, std::span<const float> src, float minus_infinity_db = -100.0f)
    {
        const size_t count = std::min(dest.size(), src.size());
        for (size_t i = 0; i < count; ++i)
        {
            dest[i] = juce::Decibels::gainToDecibels(src[i], minus_infinity_db);
        }
    }
    static void multiply(std::span<float> data, float factor)
    {
        const size_t size = data.size();
        if (factor == 1.0f) return;
        if (factor == 0.0f) {
            std::fill(data.begin(), data.end(), 0.0f);
            return;
        }
        size_t i = 0;
        __m256 v_factor = _mm256_set1_ps(factor);
        for (; i + vSize <= size; i += vSize)
        {
            __m256 v_data = _mm256_loadu_ps(&data[i]);
            _mm256_storeu_ps(&data[i], _mm256_mul_ps(v_data, v_factor));
        }
        for (; i < size; ++i)
        {
            data[i] *= factor;
        }
    }
}
