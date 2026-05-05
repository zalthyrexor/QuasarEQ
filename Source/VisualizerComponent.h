#pragma once

#include <JuceHeader.h>
#include "juce_LookAndFeel.h"
#include "juce_PluginProcessor.h"
#include "PathProducer.h"
#include "zlth_dsp_fft_resampler510.h"

class VisualizerComponent: public juce::Component, private juce::AsyncUpdater {
public:
  VisualizerComponent(QuasarEQAudioProcessor& p): audioProcessor(p), pathProducer(audioProcessor.channelFifo), analyzerThread(pathProducer, *this) {
  }
  static float getButterworthQ(int k, int n) {
    return 2.0f * std::sin((std::numbers::pi_v<float> *(2.0f * k + 1.0f)) / (2.0f * n));
  }
  void paint(juce::Graphics& g) override {
    g.drawImageAt(gridCache, 0, 0);

    SpectrumRenderData localPath;
    {
      juce::ScopedLock lock(pathLock);
      localPath = channelPathToDraw;
    }

    g.saveState();
    g.reduceClipRegion(getLevelMeterArea());

    auto meterArea = getLevelMeterArea().toFloat();
    const float peakLineThickness = 1.5f;
    const int numMeters = 4;
    const float stepW = meterArea.getWidth() / numMeters;

    for (int i = 0; i < numMeters; ++i) {
      auto x = meterArea.getX() + (stepW * i);
      auto h = remap(localPath.meterLevelsDb[i], config::METER_MIN, config::METER_MAX, meterArea.getBottom(), meterArea.getY());
      auto p = remap(localPath.meterLevelsPeakDb[i], config::METER_MIN, config::METER_MAX, meterArea.getBottom(), meterArea.getY());
      g.setColour(config::theme.withAlpha(0.55f));
      g.fillRect(juce::Rectangle<float>::leftTopRightBottom(x, h, x + stepW, meterArea.getBottom()));
      g.setColour(config::theme);
      g.fillRect(x, p - peakLineThickness * 0.5f, stepW, peakLineThickness);
    }
    g.restoreState();
    g.saveState();
    g.reduceClipRegion(getCurveArea());

    g.setColour(config::theme.withAlpha(0.45f));
    g.fillPath(spectrumDb);
    g.setColour(config::theme);
    g.strokePath(spectrumPeakDb, juce::PathStrokeType(1.5f));
    g.setColour(config::side);
    g.strokePath(responseCurvePath[1], juce::PathStrokeType(2.5f));
    g.setColour(config::theme);
    g.strokePath(responseCurvePath[0], juce::PathStrokeType(2.5f));

    g.restoreState();

    auto spectrumArea = getCurveArea().toFloat();
    auto spectrumAreaX = spectrumArea.getX();
    auto spectrumAreaY = spectrumArea.getY();
    auto spectrumAreaW = spectrumArea.getWidth();
    auto spectrumAreaB = spectrumArea.getBottom();
    for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
      if (getBandParamValue(config::ID_BYPASS, i) > 0.5f) continue;
      float x = editorFreqToCurveArea(getBandParamValue(config::ID_FREQ, i));
      float y = editorGainToCurveArea(getBandParamValue(config::ID_GAIN, i));
      const int pointSize = 14;
      g.setColour(config::textBackground);
      g.fillEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize);
      g.setColour(config::text);
      g.drawEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize, 1.5f);
      g.setFont(12.0f);
      drawLabel(g, config::IndexToID(i), x, y);
    }
  }

  int findNextAvailableBand() const {
    for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
      if (getBandParamValue(config::ID_BYPASS, i) > 0.5f) {
        return i;
      }
    }
    return NoBandSelected;
  }

  void setParamToDraggingBand(const juce::String& paramID, float plainValue) {
    auto id = config::toBiquadID(paramID, draggingBand);
    if (auto* p = audioProcessor.apvts.getParameter(id)) {
      auto range = audioProcessor.apvts.getParameterRange(id);
      p->setValueNotifyingHost(range.convertTo0to1(juce::jlimit(range.start, range.end, plainValue)));
    }
  }
  void mouseDown(const juce::MouseEvent& e) override {
    if (!getCurveArea().contains(e.getPosition())) {
      draggingBand = NoBandSelected;
      return;
    }
    draggingBand = getClosestBand(e.position);
    if (draggingBand != NoBandSelected) return;
    int availableIdx = findNextAvailableBand();
    if (availableIdx == NoBandSelected) return;
    draggingBand = availableIdx;

    auto [mouseX, mouseY] = e.position;
    auto bounds = getCurveArea().toFloat();
    float freqHz = curveAreaToEditorFreq(mouseX);
    float gainDb = remap(mouseY, bounds.getBottom(), bounds.getY(), config::PARAM_GAIN_DB_MIN, config::PARAM_GAIN_DB_MAX);
    float filterMode = getFilterModeCallback ? (float)getFilterModeCallback() : config::PARAM_FILTER_DEFAULT;
    float channelMode = getChannelModeCallback ? (float)getChannelModeCallback() : config::PARAM_CHANNEL_DEFAULT;

    setParamToDraggingBand(config::ID_BYPASS, 0.0f);
    setParamToDraggingBand(config::ID_FREQ, freqHz);
    setParamToDraggingBand(config::ID_GAIN, gainDb);
    setParamToDraggingBand(config::ID_FILTER_SHAPE, filterMode);
    setParamToDraggingBand(config::ID_CHANNEL_MODE, channelMode);
    audioProcessor.resetParam(config::toBiquadID(config::ID_Q, draggingBand));
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    if (draggingBand == NoBandSelected) {
      return;
    }
    auto [mouseX, mouseY] = e.position;
    auto bounds = getCurveArea().toFloat();
    float freqHz = curveAreaToEditorFreq(mouseX);
    float gainDb = remap(mouseY, bounds.getBottom(), bounds.getY(), config::PARAM_GAIN_DB_MIN, config::PARAM_GAIN_DB_MAX);
    setParamToDraggingBand(config::ID_FREQ, freqHz);
    setParamToDraggingBand(config::ID_GAIN, gainDb);
  }
  void mouseUp(const juce::MouseEvent& e) override {
    draggingBand = NoBandSelected;
  }
  void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
    if (draggingBand == NoBandSelected) {
      return;
    }
    if (auto* p = audioProcessor.apvts.getParameter(config::toBiquadID(config::ID_Q, draggingBand))) {
      p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + wheel.deltaY * 0.125f));
    }
  }
  std::function<int()> getFilterModeCallback;
  std::function<int()> getChannelModeCallback;
private:
  class AnalyzerThread: public juce::Thread {
  public:
    AnalyzerThread(PathProducer& producer, VisualizerComponent& comp): juce::Thread("FFT Analyzer Thread"), producer(producer), responseCurveComponent(comp) {
      startThread();
    };
    ~AnalyzerThread() override {
      stopThread(1000);
    };
    void run() override {
      while (!threadShouldExit()) {
        producer.process(responseCurveComponent.audioProcessor.getSampleRate());
        if (!responseCurveComponent.isUpdatePending() && producer.getNumPathsAvailable() > 0) {
          responseCurveComponent.triggerAsyncUpdate();
        }
        juce::Thread::sleep(THREAD_SLEEP_TIME);
      }
    };
  private:
    PathProducer& producer;
    VisualizerComponent& responseCurveComponent;
  };

  int getClosestBand(juce::Point<float> mousePos) {
    auto area = getCurveArea().toFloat();
    const float toleranceRadius = 12.0f;
    const float thresholdSq = toleranceRadius * toleranceRadius;
    for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
      if (getBandParamValue(config::ID_BYPASS, i) > 0.5f) continue;
      float x = editorFreqToCurveArea(getBandParamValue(config::ID_FREQ, i));
      float y = editorGainToCurveArea(getBandParamValue(config::ID_GAIN, i));
      if (mousePos.getDistanceSquaredFrom({x, y}) < thresholdSq) return i;
    }
    return NoBandSelected;
  }

  void handleAsyncUpdate() override {
    SpectrumRenderData pathData;
    bool newPathAvailable = false;
    while (pathProducer.getNumPathsAvailable() > 0) {
      if (pathProducer.getPath(pathData)) {
        newPathAvailable = true;
      }
    }
    calculateResponseCurve();
    if (newPathAvailable) {
      {
        juce::ScopedLock lock(pathLock);
        channelPathToDraw = pathData;
      }
      updateSpectrumPath(pathData.spectrumDb, spectrumPoints, spectrumDb, true);
      updateSpectrumPath(pathData.spectrumPeakDb, peakHoldPoints, spectrumPeakDb, false);
      repaint();
    }
  }
  void drawLabel(juce::Graphics& g, const juce::String& text, int x, int y) const {
    g.drawText(text, juce::Rectangle<int>(labelMargin, labelMargin).withCentre({x, y}), juce::Justification::centred, false);
  }
  void resized() override {
    const auto meterArea = getLevelMeterArea().toFloat();
    const auto curveArea = getCurveArea().toFloat();
    gridCache = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g(gridCache);
    g.setColour(juce::Colours::black);
    g.fillRect(curveArea);
    g.fillRect(meterArea);
    auto formatFreq = [](float f) { return (f < 1000.0f) ? juce::String((int)f) : juce::String((int)(f / 1000.0f)) + "k"; };
    auto formatDb = [](float db) { return (db > 0 ? "+" : "") + juce::String((int)db); };

    juce::Colour gridColour {0x70707070};
    juce::Colour gridColour0db {0x90909090};

    for (auto f : config::frequencies) {
      int x = editorFreqToCurveArea(f);
      g.setColour(gridColour);
      g.drawVerticalLine(x, curveArea.getY(), curveArea.getBottom());
      g.setColour(config::text);
      g.setFont(textSize);
      drawLabel(g, formatFreq(f), x, (int)curveArea.getBottom() + textSize);
      drawLabel(g, formatFreq(f), x, (int)curveArea.getY() - textSize);
    }
    for (auto db : config::editorDBs) {
      int y = editorGainToCurveArea(db);
      g.setColour(db == 0.0f ? gridColour0db : gridColour);
      g.drawHorizontalLine(y, curveArea.getX(), curveArea.getRight());
      g.setColour(config::text);
      drawLabel(g, formatDb(db), (int)curveArea.getX() - textSize, y);
    }

    for (auto db : config::meterDBs) {
      int y = remap(db, config::METER_MAX, config::METER_MIN, meterArea.getY(), meterArea.getBottom());
      g.setColour(db == 0.0f ? gridColour0db : gridColour);
      g.drawHorizontalLine(y, meterArea.getX(), meterArea.getRight());
      g.setColour(config::text);
      drawLabel(g, formatDb(db), (int)meterArea.getX() - textSize, y);
    }

    g.setColour(config::text);
    const juce::String labels[] = {"M", "S", "L", "R"};
    const float meterWidth = meterArea.getWidth() * 0.25f;
    for (int i = 0; i < 4; ++i) {
      float centerX = meterArea.getX() + meterWidth * i + meterWidth / 2.0f;
      g.setFont(textSize);
      drawLabel(g, labels[i], centerX, meterArea.getBottom() + textSize);
      drawLabel(g, labels[i], centerX, meterArea.getY() - textSize);
    }

    g.setColour(gridColour);
    for (int i = 0; i < 5; ++i) {
      float ratio = i * 0.25f;
      int xPos = (int)(meterArea.getX() + meterArea.getWidth() * ratio);
      g.drawVerticalLine(xPos, meterArea.getY(), meterArea.getBottom());
    }
  }

  juce::Rectangle<int> getLevelMeterArea() {
    auto a = getLocalBounds().removeFromRight(146);
    return a.reduced(labelMargin).reduced(margin);
  }
  juce::Rectangle<int> getCurveArea() {
    auto a = getLocalBounds();
    a.removeFromRight(146);
    return a.reduced(labelMargin).reduced(margin);
  }


  std::array<juce::Path, 2> responseCurvePath;
  std::vector<float> tanTable;
  std::array<std::vector<float>, config::CHANNEL_COUNT> curvePoints {};

  void calculateResponseCurve() {
    auto sampleRate = static_cast<float>(audioProcessor.getSampleRate());
    auto size = getCurveArea().getWidth();

    tanTable.assign(size, 0.0f);
    for (int i = 0; i < size; ++i) {
      float freq = mapToLog(i, 0, size, config::PARAM_FREQ_HZ_MIN, config::PARAM_FREQ_HZ_MAX);
      tanTable[i] = std::tan(std::numbers::pi_v<float> *std::min(freq / sampleRate, 0.4999f));
    }
    
    int maxSize = (int)std::floor(mapFromLog(std::min(config::PARAM_FREQ_HZ_MAX, sampleRate / 2.0f), config::PARAM_FREQ_HZ_MIN, config::PARAM_FREQ_HZ_MAX, 0, size));

    for (int i = 0; i < config::CHANNEL_COUNT; ++i){
      responseCurvePath[i].clear();
      curvePoints[i].assign(size, 1.0f);
    }

    auto bounds = getCurveArea().toFloat();
    auto& apvts = audioProcessor.apvts;

    for (int i = 0; i < config::BIQUAD_COUNT; ++i) {
      auto load = [&](config::BandAddressEnum index) {
        return audioProcessor.biquadTable[i][(int)index]->load();
      };
      auto l0 = load(config::BandAddressEnum::freq);
      auto l1 = load(config::BandAddressEnum::gain);
      auto l2 = load(config::BandAddressEnum::q);
      auto l3 = load(config::BandAddressEnum::bypass);
      auto l4 = load(config::BandAddressEnum::shape);
      auto l5 = load(config::BandAddressEnum::channel);
      auto p0 = zlth::unit::prewarp(l0 / sampleRate);
      auto p2 = zlth::unit::dbToMagFourthRoot(l1);
      auto p1 = zlth::unit::inverseQ(l2);
      bool isActive = l3 < 0.5f;
      auto mode = static_cast<int>(l5);
      auto type = static_cast<config::FilterType>((int)l4);
      auto b0 = (isActive && (mode == 0 || mode == 1)) ? type : config::FilterType::PassThrough;
      auto b1 = (isActive && (mode == 0 || mode == 2)) ? type : config::FilterType::PassThrough;
      for (int j = 0; j < maxSize; ++j) {
        curvePoints[0][j] *= std::norm(zlth::dsp::Filter::get_response(tanTable[j], b0, p0, p1, p2));
        curvePoints[1][j] *= std::norm(zlth::dsp::Filter::get_response(tanTable[j], b1, p0, p1, p2));
      }
    }

    for (int i = 0; i < maxSize; ++i) {
      float x = remap(i, 0, size - 1, bounds.getX(), bounds.getRight());
      auto draw = [&](int j) {
        auto pos = editorGainToCurveArea(zlth::unit::magSqToDB(curvePoints[j][i]));
        if (i == 0) {
          responseCurvePath[j].startNewSubPath(x, pos);
        }
        else {
          responseCurvePath[j].lineTo(x, pos);
        }
      };
      draw(0);
      draw(1);
    }
  }

  void updateSpectrumPath(const auto& sourcePath, std::vector<juce::Point<float>>& targetPoints, juce::Path& targetPath, bool shouldClosePath) {
    auto spectrumArea = getCurveArea().toFloat();
    auto spectrumAreaX = spectrumArea.getX();
    auto spectrumAreaY = spectrumArea.getY();
    auto spectrumAreaW = spectrumArea.getWidth();
    auto spectrumAreaB = spectrumArea.getBottom();
    targetPoints.clear();
    targetPath.clear();
    if (sourcePath.size() != 2048) return;
    targetPoints.reserve(sourcePath.size());
    auto resampled = resampler.resample(sourcePath.data(), audioProcessor.getSampleRate());
    for (int i = 1; i < resampled.size(); ++i) {
      float x = editorFreqToCurveArea(resampled[i].first);
      float y = remap(resampled[i].second, config::FFT_MIN_DB, config::FFT_MAX_DB, spectrumAreaB, spectrumAreaY);
      targetPoints.emplace_back(x, y);
    }
    targetPath.startNewSubPath(targetPoints[0]);
    targetPath.lineTo(targetPoints[1]);
    static constexpr float BEZIER_SCALE = 1.0f / 6.0f;
    for (size_t i = 1; i < 254; ++i) {
      const auto& p0 = targetPoints[i - 1];
      const auto& p1 = targetPoints[i];
      const auto& p2 = targetPoints[i + 1];
      const auto& p3 = targetPoints[i + 2];
      const juce::Point<float> cp1 = p1 + (p2 - p0) * BEZIER_SCALE;
      const juce::Point<float> cp2 = p2 - (p3 - p1) * BEZIER_SCALE;
      targetPath.cubicTo(cp1, cp2, p2);
    }
    for (size_t i = 255; i < targetPoints.size(); ++i) {
      targetPath.lineTo(targetPoints[i]);
    }
    if (!shouldClosePath) return;
    targetPath.lineTo(targetPoints.back().x, spectrumAreaB);
    targetPath.lineTo(targetPoints.front().x, spectrumAreaB);
    targetPath.closeSubPath();
  }
  float getBandParamValue(const juce::String& prefix, int bandIdx) const {
    return audioProcessor.apvts.getRawParameterValue(config::toBiquadID(prefix, bandIdx))->load();
  }

  static float norm(float v, float min, float max) {
    return (v - min) / (max - min);
  }
  static float remap(float v, float sMin, float sMax, float tMin, float tMax) {
    return tMin + (tMax - tMin) * norm(v, sMin, sMax);
  }
  static float mapFromLog(float v, float sMin, float sMax, float tMin, float tWidth) {
    return tMin + tWidth * norm(std::log(v), std::log(sMin), std::log(sMax));
  }
  static float mapToLog(float v, float sMin, float sWidth, float tMin, float tMax) {
    return std::exp(remap(v, sMin, sMin + sWidth, std::log(tMin), std::log(tMax)));
  }
  float curveAreaToEditorFreq(float v) {
    const auto curveArea = getCurveArea().toFloat();
    return mapToLog(v, curveArea.getX(), curveArea.getWidth(), config::PARAM_FREQ_HZ_MIN, config::PARAM_FREQ_HZ_MAX);
  }
  float editorFreqToCurveArea(float v) {
    const auto curveArea = getCurveArea().toFloat();
    return mapFromLog(v, config::PARAM_FREQ_HZ_MIN, config::PARAM_FREQ_HZ_MAX, curveArea.getX(), curveArea.getWidth());
  }
  float editorGainToCurveArea(float v) {
    const auto curveArea = getCurveArea().toFloat();
    return remap(v, config::PARAM_GAIN_DB_MIN, config::PARAM_GAIN_DB_MAX, curveArea.getBottom(), curveArea.getY());
  }
  static constexpr int textSize = 10;
  static constexpr int THREAD_SLEEP_TIME = 25;
  static constexpr int labelMargin = textSize * 2;
  static constexpr int margin = 2;

  int draggingBand = NoBandSelected;

  QuasarEQAudioProcessor& audioProcessor;
  PathProducer pathProducer;
  SpectrumRenderData channelPathToDraw;
  AnalyzerThread analyzerThread;
  juce::Image gridCache;
  juce::CriticalSection pathLock;
  zlth::dsp::fft::Resampler510 resampler;

  std::vector<juce::Point<float>> spectrumPoints;
  std::vector<juce::Point<float>> peakHoldPoints;

  juce::Path spectrumDb;
  juce::Path spectrumPeakDb;

  static constexpr int NoBandSelected = -1;
};
