#pragma once

#include <immintrin.h>
#include <ranges>
#include <algorithm>
#include <span>
#include <concepts>
#include <cmath>
#include <cassert>

namespace zlth::simd
{
    static constexpr int vSize = sizeof(__m256) / sizeof(float);
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

    static void multiply_inplace(std::span<float> data, float factor)
    {
        assert(data.size() % vSize == 0);
        size_t size = data.size();
        __m256 v_factor = _mm256_set1_ps(factor);
        for (size_t i = 0; i < size; i += vSize)
        {
            _mm256_storeu_ps(&data[i], _mm256_mul_ps(_mm256_loadu_ps(&data[i]), v_factor));
        }
    }
    static void complex_mag_sq(std::span<float> dest, std::span<const float> real, std::span<const float> imag)
    {
        assert(dest.size() % vSize == 0);
        assert(dest.size() == real.size());
        assert(dest.size() == imag.size());
        const size_t size = dest.size();
        for (size_t i = 0; i < size; i += vSize)
        {
            __m256 v_real = _mm256_loadu_ps(&real[i]);
            __m256 v_imag = _mm256_loadu_ps(&imag[i]);
            _mm256_storeu_ps(&dest[i], _mm256_fmadd_ps(v_imag, v_imag, _mm256_mul_ps(v_real, v_real)));
        }
    }
    static void multiply_two_buffers(std::span<float> dest, std::span<const float> src)
    {
        assert(dest.size() % vSize == 0);
        assert(dest.size() == src.size());
        const size_t size = dest.size();
        for (size_t i = 0; i < size; i += vSize)
        {
            _mm256_storeu_ps(&dest[i], _mm256_mul_ps(_mm256_loadu_ps(&dest[i]), _mm256_loadu_ps(&src[i])));
        }
    }

    [[msvc::forceinline]] static void weighted_hadamard_transform(std::span<float> span1, std::span<float> span2, float weight1, float weight2)
    {
        const size_t count = std::min(span1.size(), span2.size());
        const __m256 v_weight1 = _mm256_set1_ps(weight1);
        const __m256 v_weight2 = _mm256_set1_ps(weight2);
        size_t i = 0;
        for (; i + vSize <= count; i += vSize)
        {
            __m256 vL = _mm256_loadu_ps(&span1[i]);
            __m256 vR = _mm256_loadu_ps(&span2[i]);
            _mm256_storeu_ps(&span1[i], _mm256_mul_ps(_mm256_add_ps(vL, vR), v_weight1));
            _mm256_storeu_ps(&span2[i], _mm256_mul_ps(_mm256_sub_ps(vL, vR), v_weight2));
        }
        for (; i < count; ++i)
        {
            float l = span1[i];
            float r = span2[i];
            span1[i] = (l + r) * weight1;
            span2[i] = (l - r) * weight2;
        }
    }
    [[msvc::forceinline]] static void hadamard_transform(std::span<float> span1, std::span<float> span2)
    {
        const size_t count = std::min(span1.size(), span2.size());
        size_t i = 0;
        for (; i + vSize <= count; i += vSize)
        {
            __m256 vL = _mm256_loadu_ps(&span1[i]);
            __m256 vR = _mm256_loadu_ps(&span2[i]);
            _mm256_storeu_ps(&span1[i], _mm256_add_ps(vL, vR));
            _mm256_storeu_ps(&span2[i], _mm256_sub_ps(vL, vR));
        }
        for (; i < count; ++i)
        {
            float l = span1[i];
            float r = span2[i];
            span1[i] = l + r;
            span2[i] = l - r;
        }
    }
}

// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#avxnewtechs=AVX2
