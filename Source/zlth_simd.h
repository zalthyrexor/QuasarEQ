#pragma once
#include <immintrin.h>
#include <ranges>
#include <algorithm>
#include <span>
#include <concepts>
#include <memory>

namespace zlth::simd
{
    template<typename T>
    concept FloatType = std::same_as<T, float>;
    static constexpr int vSize = sizeof(__m512) / sizeof(float);
    static void average_two_buffers(std::span<float> dest, std::span<const float> src1, std::span<const float> src2)
    {
        __m512 v_half = _mm512_set1_ps(0.5f);
        size_t i = 0;
        const size_t size = dest.size();
        const size_t count = std::min({size, src1.size(), src2.size()});
        for (; i + vSize <= count; i += vSize)
        {
            __m512 s1 = _mm512_loadu_ps(&src1[i]);
            __m512 s2 = _mm512_loadu_ps(&src2[i]);
            _mm512_storeu_ps(&dest[i], _mm512_mul_ps(_mm512_add_ps(s1, s2), v_half));
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
            __m512 d = _mm512_loadu_ps(&dest[i]);
            __m512 s = _mm512_loadu_ps(&src[i]);
            _mm512_storeu_ps(&dest[i], _mm512_mul_ps(d, s));
        }
        for (; i < count; ++i)
        {
            dest[i] *= src[i];
        }
    }
    static void apply_falloff(std::span<float> dest, std::span<const float> src, float fallAmount)
    {
        const size_t count = std::min(dest.size(), src.size());
        const __m512 v_fall = _mm512_set1_ps(fallAmount);
        size_t i = 0;
        for (; i + vSize <= count; i += vSize)
        {
            __m512 d = _mm512_loadu_ps(&dest[i]);
            __m512 s = _mm512_loadu_ps(&src[i]);
            d = _mm512_sub_ps(d, v_fall);
            d = _mm512_max_ps(s, d);
            _mm512_storeu_ps(&dest[i], d);
        }
        for (; i < count; ++i)
        {
            dest[i] = std::max(src[i], dest[i] - fallAmount);
        }
    }
    static void gains_to_decibels(std::span<float> dest, std::span<const float> src, float minus_infinity_db = -100.0f)
    {
        const size_t count = std::min(dest.size(), src.size());
        const __m512 v_20 = _mm512_set1_ps(20.0f);
        const __m512 v_min_db = _mm512_set1_ps(minus_infinity_db);
        const __m512 v_threshold = _mm512_set1_ps(1e-5f);
        size_t i = 0;
        for (; i + vSize <= count; i += vSize)
        {
            __m512 s = _mm512_loadu_ps(&src[i]);
            s = _mm512_max_ps(s, v_threshold);
            __m512 log_s = _mm512_log10_ps(s);
            __m512 db = _mm512_mul_ps(v_20, log_s);
            db = _mm512_max_ps(db, v_min_db);
            _mm512_storeu_ps(&dest[i], db);
        }
        for (; i < count; ++i)
        {
            dest[i] = juce::Decibels::gainToDecibels(src[i], minus_infinity_db);
        }
    }
    static void multiply(std::span<float> data, float factor)
    {
        const size_t size = data.size();
        if (factor == 1.0f) return;
        if (factor == 0.0f)
        {
            std::fill(data.begin(), data.end(), 0.0f);
            return;
        }
        size_t i = 0;
        __m512 v_factor = _mm512_set1_ps(factor);
        for (; i <= size - vSize; i += vSize)
        {
            __m512 v_data = _mm512_loadu_ps(&data[i]);
            v_data = _mm512_mul_ps(v_data, v_factor);
            _mm512_storeu_ps(&data[i], v_data);
        }
        for (; i < size; ++i)
        {
            data[i] *= factor;
        }
    }
}
