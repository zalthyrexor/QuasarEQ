#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <span>
#include "forceinline.h"

namespace zlth::dsp {
  class Filter final {
  public:
    Filter() = default;
    ~Filter() = default;

    enum class FilterType {
      HighPass, LowPass, HighShelf, LowShelf, Tilt, Bell, Notch, BandPass, PassThrough
    };

    FORCEINLINE static std::complex<float> get_response(float freq, FilterType filter_, float p0, float p1, float p2) noexcept {
      if (filter_ == FilterType::PassThrough) {
        return {1.0f, 0.0f};
      }
      float lg {};
      float lk {};
      float lm0 {};
      float lm1 {};
      float lm2 {};
      calculate_coefficients(filter_, p0, p1, p2, [&](float g_, float k_, float m0_, float m1_, float m2_) noexcept {
        lg = g_;
        lk = k_;
        lm0 = m0_;
        lm1 = m1_;
        lm2 = m2_;
      });
      std::complex<float> s {0.0f, freq / lg};
      return lm0 + (lm1 * s + lm2) / (1.0f + s * (s + lk));
    }

    FORCEINLINE void process(std::span<float> span) noexcept {
      if (std::exchange(crossfade_state, false)) {
        if (cf == FilterType::PassThrough) {
          ic1 = 0.0f;
          ic2 = 0.0f;
        }
        process_span2(span);
      }
      else if (cf != FilterType::PassThrough && std::exchange(lerp_state, false)) {
        process_span1(span);
      }
      else if (cf != FilterType::PassThrough) {
        process_span0(span);
      }
    }

    FORCEINLINE void set_filter_type(zlth::dsp::Filter::FilterType t) {
      if (tf == t) {
        return;
      }
      crossfade_state = true;
      tf = t;
    }

    FORCEINLINE void set_coefficients(float p0, float p1, float p2) noexcept {
      lerp_state = true;
      tp0 = p0;
      tp1 = p1;
      tp2 = p2;
    }

  private:

    template <typename Setter>
    FORCEINLINE static void calculate_coefficients(FilterType filter_, float g_, float k_, float a_, Setter&& set) noexcept {
      switch (filter_) {
        case FilterType::LowPass:
        {
          set(g_, k_, 0.0f, 0.0f, 1.0f);
          break;
        }
        case FilterType::HighPass:
        {
          set(g_, k_, 1.0f, -k_, -1.0f);
          break;
        }
        case FilterType::Notch:
        {
          set(g_, k_, 1.0f, -k_, 0.0f);
          break;
        }
        case FilterType::BandPass:
        {
          set(g_, k_, 0.0f, k_, 0.0f);
          break;
        }
        case FilterType::Bell:
        {
          const float a2 = a_ * a_;
          const float a4 = a2 * a2;
          set(g_, k_ / a2, 1.0f, k_ * (a4 - 1.0f) / a2, 0.0f);
          break;
        }
        case FilterType::LowShelf:
        {
          const float a2 = a_ * a_;
          const float a4 = a2 * a2;
          set(g_ / a_, k_, 1.0f, k_ * (a2 - 1.0f), a4 - 1.0f);
          break;
        }
        case FilterType::HighShelf:
        {
          const float a2 = a_ * a_;
          const float a4 = a2 * a2;
          set(g_ * a_, k_, a4, k_ * (a2 - a4), 1.0f - a4);
          break;
        }
        case FilterType::Tilt:
        {
          const float a2 = a_ * a_;
          const float a4 = a2 * a2;
          set(g_ * a_, k_, a2, k_ * (1.0f - a2), (1.0f - a4) / a2);
          break;
        }
        case FilterType::PassThrough:
        {
          set(g_, k_, 1.0f, 0.0f, 0.0f);
          break;
        }
        default:
        {
          set(g_, k_, 1.0f, 0.0f, 0.0f);
          break;
        }
      }
    }

    FORCEINLINE void set_current_state() noexcept {
      calculate_coefficients(cf, cp0, cp1, cp2, [&](float g_, float k_, float m0_, float m1_, float m2_) noexcept {
        g = g_;
        k = k_;
        m0 = m0_;
        m1 = m1_;
        m2 = m2_;
      });
      a1 = 1.0f / (1.0f + g * (g + k));
    }

    FORCEINLINE void process_single(float& v0) {
      const float v1 = a1 * (ic1 + g * (v0 - ic2));
      const float v2 = ic2 + g * v1;
      ic1 = 2.0f * v1 - ic1;
      ic2 = 2.0f * v2 - ic2;
      v0 = m0 * v0 + m1 * v1 + m2 * v2;
    }

    FORCEINLINE void process_span0(std::span<float> span) noexcept {
      for (auto& v0 : span) {
        process_single(v0);
      }
    }

    void process_span1(std::span<float> span) noexcept {
      const size_t size {span.size()};
      const float dp0 {(tp0 - cp0) / static_cast<float>(size)};
      const float dp1 {(tp1 - cp1) / static_cast<float>(size)};
      const float dp2 {(tp2 - cp2) / static_cast<float>(size)};
      for (size_t i = 0; i < size; ++i) {
        process_single(span[i]);
        cp0 += dp0;
        cp1 += dp1;
        cp2 += dp2;
        set_current_state();
      }
      cp0 = tp0;
      cp1 = tp1;
      cp2 = tp2;
      set_current_state();
    }

    void process_span2(std::span<float> span) noexcept {
      const size_t size {span.size()};
      const float dp0 {(tp0 - cp0) / static_cast<float>(size)};
      const float dp1 {(tp1 - cp1) / static_cast<float>(size)};
      const float dp2 {(tp2 - cp2) / static_cast<float>(size)};
      for (size_t i = 0; i < size; ++i) {
        process_single(span[i]);
        cp0 += dp0;
        cp1 += dp1;
        cp2 += dp2;
        calculate_coefficients(tf, cp0, cp1, cp2, [&](float g_, float k_, float m0_, float m1_, float m2_) noexcept {
          g = g_ * i;
          k = k_ * i;
          m0 = m0_ * i;
          m1 = m1_ * i;
          m2 = m2_ * i;
        });
        calculate_coefficients(cf, cp0, cp1, cp2, [&](float g_, float k_, float m0_, float m1_, float m2_) noexcept {
          g += g_ * (size - i);
          k += k_ * (size - i);
          m0 += m0_ * (size - i);
          m1 += m1_ * (size - i);
          m2 += m2_ * (size - i);
        });
        g /= size;
        k /= size;
        m0 /= size;
        m1 /= size;
        m2 /= size;
        a1 = 1.0f / (1.0f + g * (g + k));
      }
      cp0 = tp0;
      cp1 = tp1;
      cp2 = tp2;
      cf = tf;
      set_current_state();
    }

    float g {1.0f};
    float k {1.0f};
    float m0 {1.0f};
    float m1 {0.0f};
    float m2 {0.0f};
    float a1 {1.0f / 3.0f};
    float ic1 {0.0f};
    float ic2 {0.0f};
    float cp0 {1.0f};
    float cp1 {1.0f};
    float cp2 {1.0f};
    float tp0 {};
    float tp1 {};
    float tp2 {};
    FilterType cf {FilterType::PassThrough};
    FilterType tf {FilterType::PassThrough};
    bool lerp_state {};
    bool crossfade_state {};
  };
}
