#pragma once

#if defined(_MSC_VER)
#define ZLTH_FORCEINLINE [[msvc::forceinline]]
#elif defined(__GNUC__) || defined(__clang__)
#define ZLTH_FORCEINLINE __attribute__((always_inline)) inline
#else
#define ZLTH_FORCEINLINE inline
#endif

#include <immintrin.h>
#include <ranges>
#include <algorithm>
#include <span>
#include <concepts>
#include <cmath>
#include <cassert>

namespace zlth::simd
{
    static constexpr int LANES = sizeof(__m256) / sizeof(float);
    ZLTH_FORCEINLINE static void average_two_buffers(std::span<float> dest, std::span<const float> src1, std::span<const float> src2)
    {
        __m256 v_half = _mm256_set1_ps(0.5f);
        size_t count = std::min({dest.size(), src1.size(), src2.size()});
        size_t i = 0;
        for (; i + LANES <= count; i += LANES)
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
    ZLTH_FORCEINLINE static void apply_falloff(std::span<float> dest, std::span<const float> src, float fallAmount)
    {
        __m256 v_fall = _mm256_set1_ps(fallAmount);
        size_t count = std::min(dest.size(), src.size());
        size_t i = 0;
        for (; i + LANES <= count; i += LANES)
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
    ZLTH_FORCEINLINE static void magnitude_sqr(std::span<float> dest, std::span<const float> span0, std::span<const float> span1)
    {
        size_t size = std::min({dest.size(), span0.size(), span1.size()});
        size_t i = 0;
        for (; i + LANES <= size; i += LANES)
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
    ZLTH_FORCEINLINE static void multiply_two_buffers(std::span<float> dest, std::span<const float> src)
    {
        size_t size = std::min(dest.size(), src.size());
        size_t i = 0;
        for (; i + LANES <= size; i += LANES)
        {
            _mm256_storeu_ps(&dest[i], _mm256_mul_ps(_mm256_loadu_ps(&dest[i]), _mm256_loadu_ps(&src[i])));
        }
        for (; i < size; ++i)
        {
            dest[i] *= src[i];
        }
    }
    ZLTH_FORCEINLINE static void multiply_inplace(std::span<float> span, float factor)
    {
        __m256 v_factor = _mm256_set1_ps(factor);
        size_t size = span.size();
        size_t i = 0;
        for (; i + LANES <= size; i += LANES)
        {
            __m256 v_data = _mm256_loadu_ps(&span[i]);
            _mm256_storeu_ps(&span[i], _mm256_mul_ps(v_data, v_factor));
        }
        for (; i < size; ++i)
        {
            span[i] *= factor;
        }
    }
    ZLTH_FORCEINLINE static void hadamard_butterfly(std::span<float> span0, std::span<float> span1)
    {
        size_t count = std::min(span0.size(), span1.size());
        size_t i = 0;
        for (; i + LANES <= count; i += LANES)
        {
            __m256 vL = _mm256_loadu_ps(&span0[i]);
            __m256 vR = _mm256_loadu_ps(&span1[i]);
            _mm256_storeu_ps(&span0[i], _mm256_add_ps(vL, vR));
            _mm256_storeu_ps(&span1[i], _mm256_sub_ps(vL, vR));
        }
        for (; i < count; ++i)
        {
            float l = span0[i];
            float r = span1[i];
            span0[i] = l + r;
            span1[i] = l - r;
        }
    }
    ZLTH_FORCEINLINE static float get_max_from_m256(__m256 v)
    {
        __m128 x128 = _mm_max_ps(_mm256_extractf128_ps(v, 1), _mm256_castps256_ps128(v));
        __m128 x64 = _mm_max_ps(x128, _mm_movehl_ps(x128, x128));
        __m128 x32 = _mm_max_ps(x64, _mm_shuffle_ps(x64, x64, _MM_SHUFFLE(1, 1, 1, 1)));
        return _mm_cvtss_f32(x32);
    }
    ZLTH_FORCEINLINE static float get_magnitude_avx2(std::span<const float> data)
    {
        __m256 absMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
        __m256 maxVals = _mm256_setzero_ps();
        size_t size = data.size();
        size_t i = 0;
        for (; i + LANES <= size; i += LANES)
        {
            __m256 v = _mm256_loadu_ps(&data[i]);
            __m256 vAbs = _mm256_and_ps(v, absMask);
            maxVals = _mm256_max_ps(maxVals, vAbs);
        }
        float finalMax = get_max_from_m256(maxVals);
        for (; i < size; ++i)
        {
            finalMax = std::max(finalMax, std::abs(data[i]));
        }
        return finalMax;
    }
}
