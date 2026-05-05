#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <span>
#include "forceinline.h"
#include "config.h"
#include "unit.h"

namespace zlth::dsp {
  class Filter final {
  public:

    FORCEINLINE Filter(
      std::atomic<float>* sr_,
      std::atomic<float>* l0_, 
      std::atomic<float>* l1_,
      std::atomic<float>* l2_,
      std::atomic<float>* l3_,
      std::atomic<float>* l4_,
      std::atomic<float>* l5_,
      int ch_
    ):
      sr(sr_),
      l0(l0_),
      l1(l1_),
      l2(l2_),
      l3(l3_),
      l4(l4_),
      l5(l5_),
      ch(ch_)
    {
      auto sr__ = sr->load();
      auto l0__ = l0->load();
      auto l1__ = l1->load();
      auto l2__ = l2->load();
      auto l3__ = l3->load();
      auto l4__ = l4->load();
      auto l5__ = l5->load();
      tp0 = zlth::unit::prewarp(l0__ / sr__);
      tp1 = zlth::unit::inverseQ(l1__);
      tp2 = zlth::unit::dbToMagFourthRoot(l2__);
      tf = ((l3__ < 0.5f) && ((int)l5__ == 0 || (int)l5__ == (ch + 1))) ? (config::FilterType)(int)l4__ : config::FilterType::PassThrough;
    }

    ~Filter() = default;

    FORCEINLINE void update_curve(std::span<float> curvePoints, std::span<float> table) const noexcept {
      auto loadsr = sr->load();
      auto load0 = l0->load();
      auto load1 = l1->load();
      auto load2 = l2->load();
      auto load3 = l3->load();
      auto load4 = l4->load();
      auto load5 = l5->load();
      auto p0_ = zlth::unit::prewarp(load0 / loadsr);
      auto p1_ = zlth::unit::inverseQ(load1);
      auto p2_ = zlth::unit::dbToMagFourthRoot(load2);
      auto f_ = ((load3 < 0.5f) && ((int)load5 == 0 || (int)load5 == (ch + 1))) ? (config::FilterType)(int)load4 : config::FilterType::PassThrough;
      if (f_ == config::FilterType::PassThrough) {
        return;
      }
      float g_ {};
      float k_ {};
      float m0_ {};
      float m1_ {};
      float m2_ {};
      calculate_coefficients(f_, p0_, p1_, p2_, [&](float g__, float k__, float m0__, float m1__, float m2__) noexcept {
        g_ = g__;
        k_ = k__;
        m0_ = m0__;
        m1_ = m1__;
        m2_ = m2__;
      });
      for (int j = 0; j < curvePoints.size(); ++j) {
        std::complex<float> s {0.0f, table[j] / g_};
        curvePoints[j] *= std::norm(m0_ + (m1_ * s + m2_) / (1.0f + s * (s + k_)));
      }
    }

    FORCEINLINE void process(std::span<float> span_) noexcept {

      auto sr_ = sr->load();
      auto l0_ = l0->load();
      auto l1_ = l1->load();
      auto l2_ = l2->load();
      auto l3_ = l3->load();
      auto l4_ = l4->load();
      auto l5_ = l5->load();

      bool crossfade_ {};
      bool lerp_ {};

      if ((csr != sr_) || (last_l0 != l0_) || (last_l1 != l1_) || (last_l2 != l2_)) {
        lerp_ = true;
        csr = sr_;
        last_l0 = l0_;
        last_l1 = l1_;
        last_l2 = l2_;
        tp0 = zlth::unit::prewarp(l0_ / sr_);
        tp1 = zlth::unit::inverseQ(l1_);
        tp2 = zlth::unit::dbToMagFourthRoot(l2_);
      }
      if ((last_l3 != l3_) || (last_l4 != l4_) || (last_l5 != l5_)) {
        crossfade_ = true;
        last_l3 = l3_;
        last_l4 = l4_;
        last_l5 = l5_;
        tf = ((l3_ < 0.5f) && ((int)l5_ == 0 || (int)l5_ == (ch + 1))) ? (config::FilterType)(int)l4_ : config::FilterType::PassThrough;
      }

      if (crossfade_) {
        process_impl_crossfade(span_);
      }
      else if (lerp_ && cf != config::FilterType::PassThrough) {
        process_impl_lerp(span_);
      }
      else if (cf != config::FilterType::PassThrough) {
        for (auto& v0 : span_) {
          process_single(v0);
        }
      }
      cp0 = tp0;
      cp1 = tp1;
      cp2 = tp2;
      cf = tf;
      set_current_state();
    }

  private:

    template <typename Setter>
    FORCEINLINE static void calculate_coefficients(config::FilterType f_, float g_, float k_, float a1_, Setter&& set_) noexcept {
      switch (f_) {
        case config::FilterType::LowPass:
        {
          set_(g_, k_, 0.0f, 0.0f, 1.0f);
          break;
        }
        case config::FilterType::HighPass:
        {
          set_(g_, k_, 1.0f, -k_, -1.0f);
          break;
        }
        case config::FilterType::Notch:
        {
          set_(g_, k_, 1.0f, -k_, 0.0f);
          break;
        }
        case config::FilterType::BandPass:
        {
          set_(g_, k_, 0.0f, k_, 0.0f);
          break;
        }
        case config::FilterType::Bell:
        {
          const float a2_ {a1_ * a1_};
          const float a4_ {a2_ * a2_};
          set_(g_, k_ / a2_, 1.0f, k_ * (a4_ - 1.0f) / a2_, 0.0f);
          break;
        }
        case config::FilterType::LowShelf:
        {
          const float a2_ {a1_ * a1_};
          const float a4_ {a2_ * a2_};
          set_(g_ / a1_, k_, 1.0f, k_ * (a2_ - 1.0f), a4_ - 1.0f);
          break;
        }
        case config::FilterType::HighShelf:
        {
          const float a2_ {a1_ * a1_};
          const float a4_ {a2_ * a2_};
          set_(g_ * a1_, k_, a4_, k_ * (a2_ - a4_), 1.0f - a4_);
          break;
        }
        case config::FilterType::Tilt:
        {
          const float a2_ {a1_ * a1_};
          const float a4_ {a2_ * a2_};
          set_(g_ * a1_, k_, a2_, k_ * (1.0f - a2_), (1.0f - a4_) / a2_);
          break;
        }
        case config::FilterType::PassThrough:
        {
          set_(g_, k_, 1.0f, 0.0f, 0.0f);
          break;
        }
        default:
        {
          set_(g_, k_, 1.0f, 0.0f, 0.0f);
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
      const float v1 {a1 * (ic1 + g * (v0 - ic2))};
      const float v2 {ic2 + g * v1};
      ic1 = 2.0f * v1 - ic1;
      ic2 = 2.0f * v2 - ic2;
      v0 = m0 * v0 + m1 * v1 + m2 * v2;
    }

    void process_impl_lerp(std::span<float> span) noexcept {
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
    }

    void process_impl_crossfade(std::span<float> span) noexcept {
      if (cf == config::FilterType::PassThrough) {
        ic1 = 0.0f;
        ic2 = 0.0f;
      }
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
    config::FilterType cf {config::FilterType::PassThrough};
    config::FilterType tf {config::FilterType::PassThrough};
    int ch;
    std::atomic<float>* sr;
    std::atomic<float>* l0;
    std::atomic<float>* l1;
    std::atomic<float>* l2;
    std::atomic<float>* l3;
    std::atomic<float>* l4;
    std::atomic<float>* l5;
    float csr;
    float last_l0;
    float last_l1;
    float last_l2;
    float last_l3;
    float last_l4;
    float last_l5;
  };
}
