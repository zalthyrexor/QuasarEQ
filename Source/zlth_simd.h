#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <immintrin.h>
#include <initializer_list>
#include <span>
#include "unit.h"

namespace zlth::simd {
  using T = float;
  using Reg = __m256;
  using SpanSIMD = std::span<T>;
  using ConstSpan = std::span<const T>;
  static constexpr int step = sizeof(Reg) / sizeof(T);
  template<typename Op>
  FORCEINLINE static void apply(SpanSIMD io, T value, Op simd_op, auto scalar_op) {
    Reg v_val = _mm256_set1_ps(value);
    size_t n = io.size();
    size_t i = 0;
    for (; i + step <= n; i += step) {
      _mm256_storeu_ps(&io[i], simd_op(_mm256_loadu_ps(&io[i]), v_val));
    }
    for (; i < n; ++i) {
      scalar_op(io[i], value);
    }
  }
  template<typename Op>
  FORCEINLINE static void apply(SpanSIMD io, ConstSpan in, Op simd_op, auto scalar_op) {
    size_t n = std::min(io.size(), in.size());
    size_t i = 0;
    for (; i + step <= n; i += step) {
      _mm256_storeu_ps(&io[i], simd_op(_mm256_loadu_ps(&io[i]), _mm256_loadu_ps(&in[i])));
    }
    for (; i < n; ++i) {
      scalar_op(io[i], in[i]);
    }
  }
  FORCEINLINE static void add_inplace(SpanSIMD io, T v) { apply(io, v, [](Reg a, Reg b) { return _mm256_add_ps(a, b); }, [](T& a, T b) { a += b; }); }
  FORCEINLINE static void sub_inplace(SpanSIMD io, T v) { apply(io, v, [](Reg a, Reg b) { return _mm256_sub_ps(a, b); }, [](T& a, T b) { a -= b; }); }
  FORCEINLINE static void mul_inplace(SpanSIMD io, T v) { apply(io, v, [](Reg a, Reg b) { return _mm256_mul_ps(a, b); }, [](T& a, T b) { a *= b; }); }
  FORCEINLINE static void div_inplace(SpanSIMD io, T v) { apply(io, v, [](Reg a, Reg b) { return _mm256_div_ps(a, b); }, [](T& a, T b) { a /= b; }); }
  FORCEINLINE static void min_inplace(SpanSIMD io, T v) { apply(io, v, [](Reg a, Reg b) { return _mm256_min_ps(a, b); }, [](T& a, T b) { a = std::min(a, b); }); }
  FORCEINLINE static void max_inplace(SpanSIMD io, T v) { apply(io, v, [](Reg a, Reg b) { return _mm256_max_ps(a, b); }, [](T& a, T b) { a = std::max(a, b); }); }
  FORCEINLINE static void add_inplace(SpanSIMD io, ConstSpan in) { apply(io, in, [](Reg a, Reg b) { return _mm256_add_ps(a, b); }, [](T& a, T b) { a += b; }); }
  FORCEINLINE static void sub_inplace(SpanSIMD io, ConstSpan in) { apply(io, in, [](Reg a, Reg b) { return _mm256_sub_ps(a, b); }, [](T& a, T b) { a -= b; }); }
  FORCEINLINE static void mul_inplace(SpanSIMD io, ConstSpan in) { apply(io, in, [](Reg a, Reg b) { return _mm256_mul_ps(a, b); }, [](T& a, T b) { a *= b; }); }
  FORCEINLINE static void div_inplace(SpanSIMD io, ConstSpan in) { apply(io, in, [](Reg a, Reg b) { return _mm256_div_ps(a, b); }, [](T& a, T b) { a /= b; }); }
  FORCEINLINE static void min_inplace(SpanSIMD io, ConstSpan in) { apply(io, in, [](Reg a, Reg b) { return _mm256_min_ps(a, b); }, [](T& a, T b) { a = std::min(a, b); }); }
  FORCEINLINE static void max_inplace(SpanSIMD io, ConstSpan in) { apply(io, in, [](Reg a, Reg b) { return _mm256_max_ps(a, b); }, [](T& a, T b) { a = std::max(a, b); }); }
  FORCEINLINE static void magnitude_sqr(std::span<T> out, std::span<const T> in0, std::span<const T> in1) {
    size_t n = std::min({out.size(), in0.size(), in1.size()});
    size_t i = 0;
    for (; i + step <= n; i += step) {
      Reg v0 = _mm256_loadu_ps(&in0[i]);
      Reg v1 = _mm256_loadu_ps(&in1[i]);
      _mm256_storeu_ps(&out[i], _mm256_fmadd_ps(v1, v1, _mm256_mul_ps(v0, v0)));
    }
    for (; i < n; ++i) {
      out[i] = in0[i] * in0[i] + in1[i] * in1[i];
    }
  }
  FORCEINLINE static void lerp_inplace(std::span<T> io, std::span<const T> in, T value) {
    Reg v_val = _mm256_set1_ps(value);
    size_t n = std::min(io.size(), in.size());
    size_t i = 0;
    for (; i + step <= n; i += step) {
      Reg v0 = _mm256_loadu_ps(&io[i]);
      _mm256_storeu_ps(&io[i], _mm256_fmadd_ps(v_val, _mm256_sub_ps(_mm256_loadu_ps(&in[i]), v0), v0));
    }
    for (; i < n; ++i) {
      io[i] += value * (in[i] - io[i]);
    }
  }
  FORCEINLINE static void hadamard_butterfly(std::span<T> io0, std::span<T> io1) {
    size_t n = std::min(io0.size(), io1.size());
    size_t i = 0;
    for (; i + step <= n; i += step) {
      Reg v0 = _mm256_loadu_ps(&io0[i]);
      Reg v1 = _mm256_loadu_ps(&io1[i]);
      _mm256_storeu_ps(&io0[i], _mm256_add_ps(v0, v1));
      _mm256_storeu_ps(&io1[i], _mm256_sub_ps(v0, v1));
    }
    for (; i < n; ++i) {
      T s0 = io0[i];
      T s1 = io1[i];
      io0[i] = s0 + s1;
      io1[i] = s0 - s1;
    }
  }
  FORCEINLINE static T get_max(Reg v) {
    __m128 x128 = _mm_max_ps(_mm256_extractf128_ps(v, 1), _mm256_castps256_ps128(v));
    __m128 x64 = _mm_max_ps(x128, _mm_movehl_ps(x128, x128));
    __m128 x32 = _mm_max_ps(x64, _mm_shuffle_ps(x64, x64, _MM_SHUFFLE(1, 1, 1, 1)));
    return _mm_cvtss_f32(x32);
  }
  FORCEINLINE static T get_abs_max(std::span<const T> in) {
    Reg absMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    Reg maxVals = _mm256_setzero_ps();
    size_t n = in.size();
    size_t i = 0;
    for (; i + step <= n; i += step) {
      Reg v = _mm256_loadu_ps(&in[i]);
      Reg vAbs = _mm256_and_ps(v, absMask);
      maxVals = _mm256_max_ps(maxVals, vAbs);
    }
    T finalMax = get_max(maxVals);
    for (; i < n; ++i) {
      finalMax = std::max(finalMax, std::abs(in[i]));
    }
    return finalMax;
  }
  FORCEINLINE static void mag_to_db(std::span<T> out, std::span<const T> in) {
    size_t n = std::min(out.size(), in.size());
    for (size_t i = 0; i < n; ++i) {
      out[i] = zlth::unit::magToDB(in[i]);
    }
  }
}
