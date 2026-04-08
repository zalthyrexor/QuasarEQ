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

    static void magnitude_sqr(std::span<float> dest, std::span<const float> span0, std::span<const float> span1)
    {
        size_t size = dest.size();
        size_t i = 0;
        for (; i + vSize <= size; i += vSize)
        {
            __m256 v_0 = _mm256_loadu_ps(&span0[i]);
            __m256 v_1 = _mm256_loadu_ps(&span1[i]);
            _mm256_storeu_ps(&dest[i], _mm256_fmadd_ps(v_1, v_1, _mm256_mul_ps(v_0, v_0)));
        }
        for (; i < size; ++i)
        {
            dest[i] = (span0[i] * span0[i]) + (span1[i] * span1[i]);
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

    [[msvc::forceinline]] static void multiply_inplace(std::span<float> data, float factor)
    {
        const size_t size = data.size();
        const __m256 v_factor = _mm256_set1_ps(factor);
        size_t i = 0;
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

    [[msvc::forceinline]] static void hadamard_butterfly(std::span<float> span1, std::span<float> span2)
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

    /*
    void process_db_conversion(float* input, float* output, int N) 
    {
        constexpr float pow2_23 = 8388608.0f;
        constexpr float log10_2 = 0.30102999566f;
        constexpr float factor_val = 10.0f * log10_2 / pow2_23;
        const __m256i offset = _mm256_set1_epi32(0x3f800000);
        const __m256 factor = _mm256_set1_ps(factor_val);
        int i = 0;
        for (; i <= N - 8; i += 8)
        {
            __m256 x = _mm256_loadu_ps(&input[i]);
            __m256i int_x = _mm256_castps_si256(x);
            __m256 log2_approx = _mm256_cvtepi32_ps(_mm256_sub_epi32(int_x, offset));
            __m256 db = _mm256_mul_ps(log2_approx, factor);
            _mm256_storeu_ps(&output[i], db);
        }
        for (; i < N; ++i)
        {
            uint32_t i_val;
            std::memcpy(&i_val, &input[i], sizeof(float));
            output[i] = static_cast<float>(static_cast<int32_t>(i_val) - 0x3f800000) * factor_val;
        }
    }
    */
}

// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#avxnewtechs=AVX2
