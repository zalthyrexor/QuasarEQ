#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <span>
#include "forceinline.h"
#include "config.h"
#include "unit.h"
#include "zlth_dsp_filter_impl.h"
namespace zlth::dsp {
  class Filter final {
  public:
    FORCEINLINE Filter(
      std::atomic<float>* r0_,
      std::atomic<float>* r1_,
      std::atomic<float>* r2_,
      std::atomic<float>* r3_,
      std::atomic<float>* r4_,
      std::atomic<float>* r5_,
      std::atomic<float>* r6_,
      int ch_
    ):
      r {r0_, r1_, r2_, r3_, r4_, r5_, r6_},
      ch {ch_} {
    }
    ~Filter() = default;
    FORCEINLINE void update_curve(std::span<float> curvePoints, std::span<float> table) const noexcept {
      auto r0_ = r[PIdx::Freq]->load();
      auto r1_ = r[PIdx::Q]->load();
      auto r2_ = r[PIdx::Gain]->load();
      auto r3_ = r[PIdx::Bypass]->load();
      auto r4_ = r[PIdx::Type]->load();
      auto r5_ = r[PIdx::Chan]->load();
      auto r6_ = r[PIdx::SR]->load();
      auto p0_ = zlth::unit::prewarp(r0_ / r6_);
      auto p1_ = zlth::unit::inverseQ(r1_);
      auto p2_ = zlth::unit::dbToMagFourthRoot(r2_);
      auto p3_ = ((r3_ < 0.5f) && ((int)r5_ == 0 || (int)r5_ == (ch + 1))) ? (config::FilterType)(int)r4_ : config::FilterType::PassThrough;
      if (p3_ == config::FilterType::PassThrough) {
        return;
      }
      for (int j = 0; j < curvePoints.size(); ++j) {
        curvePoints[j] *= norm(f.get_response(table[j], p3_, p0_, p1_, p2_));
      }
    }
    FORCEINLINE void process(std::span<float> s_) noexcept {
      float r_[7] {};
      for (int i = 0; i < 7; ++i) {
        r_[i] = r[i]->load(std::memory_order_relaxed);
      }
      if (c[PIdx::Freq] != r_[PIdx::Freq] || c[PIdx::Q] != r_[PIdx::Q] || c[PIdx::Gain] != r_[PIdx::Gain] || c[PIdx::SR] != r_[PIdx::SR]) {
        auto p0_ = zlth::unit::prewarp(r_[PIdx::Freq] / r_[PIdx::SR]);
        auto p1_ = zlth::unit::inverseQ(r_[PIdx::Q]);
        auto p2_ = zlth::unit::dbToMagFourthRoot(r_[PIdx::Gain]);
        f.set_coefficients(p0_, p1_, p2_);
      }
      if (c[PIdx::Bypass] != r_[PIdx::Bypass] || c[PIdx::Type] != r_[PIdx::Type] || c[PIdx::Chan] != r_[PIdx::Chan]) {
        auto p3_ = ((r_[PIdx::Bypass] < 0.5f) && ((int)r_[PIdx::Chan] == 0 || (int)r_[PIdx::Chan] == (ch + 1))) ? (config::FilterType)(int)r_[PIdx::Type] : config::FilterType::PassThrough;
        f.set_filter_type(p3_);
      }
      f.process(s_);
      std::copy(std::begin(r_), std::end(r_), std::begin(c));
    }
  private:
    using PIdx = config::PIdx;
    zlth::dsp::FilterImpl f {};
    std::atomic<float>* r[7] {};
    float c[7] {};
    int ch {};
  };
}
