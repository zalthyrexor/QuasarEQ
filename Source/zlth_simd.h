#pragma once

#include <immintrin.h>
#include <ranges>
#include <algorithm>
#include <span>
#include <concepts>
#include <cmath>

#if defined(_MSC_VER)
#define ZLTH_FORCEINLINE [[msvc::forceinline]]
#elif defined(__GNUC__) || defined(__clang__)
#define ZLTH_FORCEINLINE __attribute__((always_inline)) inline
#else
#define ZLTH_FORCEINLINE inline
#endif

namespace zlth::simd
{
    using Scalar = float;
    using Register = __m256;
    static constexpr int step = sizeof(Register) / sizeof(Scalar);

    struct Batch
    {
        Register data;
        Batch(Register m): data(m) {}
        Batch(Scalar s): data(_mm256_set1_ps(s)) {}
        ZLTH_FORCEINLINE void store(Scalar* ptr) const { _mm256_storeu_ps(ptr, data); }
        ZLTH_FORCEINLINE static Batch load(const Scalar* ptr) { return _mm256_loadu_ps(ptr); }
        ZLTH_FORCEINLINE Batch operator+(const Batch other) const { return _mm256_add_ps(data, other.data); }
        ZLTH_FORCEINLINE Batch operator-(const Batch other) const { return _mm256_sub_ps(data, other.data); }
        ZLTH_FORCEINLINE Batch operator*(const Batch other) const { return _mm256_mul_ps(data, other.data); }
        ZLTH_FORCEINLINE Batch operator/(const Batch other) const { return _mm256_div_ps(data, other.data); }
    };
    template <typename Lambda>
    ZLTH_FORCEINLINE void forEach(std::span<Scalar> io, Lambda action)
    {
        size_t n = io.size();
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            Batch v = Batch::load(&io[i]);
            Batch result = action(v);
            result.store(&io[i]);
        }
        for (; i < n; ++i)
        {
            io[i] = action(io[i]);
        }
    }
}

namespace zlth::simd
{

    ZLTH_FORCEINLINE static void add_inplace(std::span<Scalar> io, Scalar value) { forEach(io, [value](auto v) { return v + value; }); }
    ZLTH_FORCEINLINE static void sub_inplace(std::span<Scalar> io, Scalar value) { forEach(io, [value](auto v) { return v - value; }); }
    ZLTH_FORCEINLINE static void mul_inplace(std::span<Scalar> io, Scalar value) { forEach(io, [value](auto v) { return v * value; }); }
    ZLTH_FORCEINLINE static void div_inplace(std::span<Scalar> io, Scalar value) { forEach(io, [value](auto v) { return v / value; }); }

    ZLTH_FORCEINLINE static void max_inplace(std::span<Scalar> io, Scalar value)
    {
        Register v_val = _mm256_set1_ps(value);
        size_t n = io.size();
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            _mm256_storeu_ps(&io[i], _mm256_max_ps(_mm256_loadu_ps(&io[i]), v_val));
        }
        for (; i < n; ++i)
        {
            io[i] = std::max(io[i], value);
        }
    }

    ZLTH_FORCEINLINE static void min_inplace(std::span<Scalar> io, Scalar value)
    {
        Register v_val = _mm256_set1_ps(value);
        size_t n = io.size();
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            _mm256_storeu_ps(&io[i], _mm256_min_ps(_mm256_loadu_ps(&io[i]), v_val));
        }
        for (; i < n; ++i)
        {
            io[i] = std::min(io[i], value);
        }
    }

    ZLTH_FORCEINLINE static void max_inplace(std::span<Scalar> io, std::span<const Scalar> in)
    {
        size_t n = std::min(io.size(), in.size());
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            _mm256_storeu_ps(&io[i], _mm256_max_ps(_mm256_loadu_ps(&io[i]), _mm256_loadu_ps(&in[i])));
        }
        for (; i < n; ++i)
        {
            io[i] = std::max(io[i], in[i]);
        }
    }

    ZLTH_FORCEINLINE static void min_inplace(std::span<Scalar> io, std::span<const Scalar> in)
    {
        size_t n = std::min(io.size(), in.size());
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            _mm256_storeu_ps(&io[i], _mm256_min_ps(_mm256_loadu_ps(&io[i]), _mm256_loadu_ps(&in[i])));
        }
        for (; i < n; ++i)
        {
            io[i] = std::min(io[i], in[i]);
        }
    }

    ZLTH_FORCEINLINE static void magnitude_sqr(std::span<Scalar> out, std::span<const Scalar> in0, std::span<const Scalar> in1)
    {
        size_t n = std::min({out.size(), in0.size(), in1.size()});
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            Register v0 = _mm256_loadu_ps(&in0[i]);
            Register v1 = _mm256_loadu_ps(&in1[i]);
            _mm256_storeu_ps(&out[i], _mm256_fmadd_ps(v1, v1, _mm256_mul_ps(v0, v0)));
        }
        for (; i < n; ++i)
        {
            out[i] = in0[i] * in0[i] + in1[i] * in1[i];
        }
    }

    ZLTH_FORCEINLINE static void hadamard_product(std::span<Scalar> io, std::span<const Scalar> in)
    {
        size_t n = std::min(io.size(), in.size());
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            _mm256_storeu_ps(&io[i], _mm256_mul_ps(_mm256_loadu_ps(&io[i]), _mm256_loadu_ps(&in[i])));
        }
        for (; i < n; ++i)
        {
            io[i] *= in[i];
        }
    }

    ZLTH_FORCEINLINE static void lerp_inplace(std::span<Scalar> io, std::span<const Scalar> in, Scalar value)
    {
        Register v_val = _mm256_set1_ps(value);
        size_t n = std::min(io.size(), in.size());
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            Register v0 = _mm256_loadu_ps(&io[i]);
            _mm256_storeu_ps(&io[i], _mm256_fmadd_ps(v_val, _mm256_sub_ps(_mm256_loadu_ps(&in[i]), v0), v0));
        }
        for (; i < n; ++i)
        {
            io[i] += value * (in[i] - io[i]);
        }
    }

    ZLTH_FORCEINLINE static void hadamard_butterfly(std::span<Scalar> io0, std::span<Scalar> io1)
    {
        size_t n = std::min(io0.size(), io1.size());
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            Register v0 = _mm256_loadu_ps(&io0[i]);
            Register v1 = _mm256_loadu_ps(&io1[i]);
            _mm256_storeu_ps(&io0[i], _mm256_add_ps(v0, v1));
            _mm256_storeu_ps(&io1[i], _mm256_sub_ps(v0, v1));
        }
        for (; i < n; ++i)
        {
            Scalar s0 = io0[i];
            Scalar s1 = io1[i];
            io0[i] = s0 + s1;
            io1[i] = s0 - s1;
        }
    }

    ZLTH_FORCEINLINE static Scalar get_max(Register v)
    {
        __m128 x128 = _mm_max_ps(_mm256_extractf128_ps(v, 1), _mm256_castps256_ps128(v));
        __m128 x64 = _mm_max_ps(x128, _mm_movehl_ps(x128, x128));
        __m128 x32 = _mm_max_ps(x64, _mm_shuffle_ps(x64, x64, _MM_SHUFFLE(1, 1, 1, 1)));
        return _mm_cvtss_f32(x32);
    }

    ZLTH_FORCEINLINE static Scalar get_abs_max(std::span<const Scalar> in)
    {
        Register absMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
        Register maxVals = _mm256_setzero_ps();
        size_t n = in.size();
        size_t i = 0;
        for (; i + step <= n; i += step)
        {
            Register v = _mm256_loadu_ps(&in[i]);
            Register vAbs = _mm256_and_ps(v, absMask);
            maxVals = _mm256_max_ps(maxVals, vAbs);
        }
        Scalar finalMax = get_max(maxVals);
        for (; i < n; ++i)
        {
            finalMax = std::max(finalMax, std::abs(in[i]));
        }
        return finalMax;
    }

    ZLTH_FORCEINLINE static void log10(std::span<Scalar> out, std::span<Scalar> in)
    {
        size_t n = std::min(out.size(), in.size());
        for (size_t i = 0; i < n; ++i)
        {
            out[i] = std::log10(in[i]);
        }
    }
}
