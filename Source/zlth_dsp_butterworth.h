#pragma once

#include "zlth_dsp_filter_impl.h"

namespace zlth::dsp {
  class Butterworth final {
  public:

    FORCEINLINE Butterworth(
      std::atomic<float>* r0_,
      std::atomic<float>* r2_,
      std::atomic<float>* r3_,
      std::atomic<float>* r4_,
      std::atomic<float>* r5_,
      std::atomic<float>* r6_,
      int ch_
    ):
      r {r0_, r2_, r3_, r4_, r5_, r6_},
      ch {ch_} {
    }
    static float getButterworthQ(int k, int n) {
      return 2.0f * std::sin((std::numbers::pi_v<float> *(2.0f * k + 1.0f)) / (2.0f * n));
    }
    FORCEINLINE void update_curve(std::span<float> curvePoints, std::span<float> table) const noexcept {
      auto r0_ = r[0]->load();
      auto r2_ = r[1]->load();
      auto r3_ = r[2]->load();
      auto r4_ = r[3]->load();
      auto r5_ = r[4]->load();
      auto r6_ = r[5]->load();
      auto p0_ = zlth::unit::prewarp(r0_ / r6_);
      auto p2_ = zlth::unit::dbToMagFourthRoot(r2_);
      auto p3_ = ((r3_ < 0.5f) && ((int)r5_ == 0 || (int)r5_ == (ch + 1))) ? (config::FilterType)(int)r4_ : config::FilterType::PassThrough;

      for(int i = 0; i < r2_; ++i){
        auto p1_ = getButterworthQ(i, r2_);
        for (int j = 0; j < curvePoints.size(); ++j) {
          curvePoints[j] *= norm(f.get_response(table[j], p3_, p0_, p1_, p2_));
        }
      }
    }
    FORCEINLINE void process(std::span<float> s_) noexcept {
    }
  private:
    zlth::dsp::FilterImpl f {};
    std::atomic<float>* r[6] {};
    float c[6] {};
    int ch {};
  };
}
