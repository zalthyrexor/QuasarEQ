#pragma once

#include <JuceHeader.h>
#include "juce_LookAndFeel.h"
#include "juce_PluginProcessor.h"
#include "PathProducer.h"
#include "zlth_dsp_fft_resampler510.h"

class VisualizerComponent: public juce::Component, private juce::AsyncUpdater, public juce::AudioProcessorValueTreeState::Listener
{
public:
    VisualizerComponent(QuasarEQAudioProcessor& p): audioProcessor(p), pathProducer(audioProcessor.channelFifo), analyzerThread(pathProducer, *this) {
        for (int i = 0; i < config::BAND_COUNT; ++i) {
            for (const auto& prefix : config::bandParamPrefixes) {
                audioProcessor.apvts.addParameterListener(config::getID(prefix, i), this);
            }
        }
    }
    ~VisualizerComponent() {
        for (int i = 0; i < config::BAND_COUNT; ++i) {
            for (const auto& prefix : config::bandParamPrefixes) {
                audioProcessor.apvts.removeParameterListener(config::getID(prefix, i), this);
            }
        }
    }
    int IndexToID(int bandIdx) const {
        return bandIdx + 1;
    }
    void parameterChanged(const juce::String& parameterID, float newValue) {
        parametersNeedUpdate = true;
    }
    void paint(juce::Graphics& g) override {
        g.drawImageAt(gridCache, 0, 0);

        SpectrumRenderData localPath;
        {
            juce::ScopedLock lock(pathLock);
            localPath = channelPathToDraw;
        }

        auto meterArea = getLevelMeterArea().toFloat();
        auto meterAreaX = meterArea.getX();
        auto meterAreaY = meterArea.getY();
        auto meterAreaW = meterArea.getWidth();
        auto meterAreaB = meterArea.getBottom();
        auto meterHeightM = juce::jmap(juce::jlimit(config::METER_MIN, config::METER_MAX, localPath.meterLevelsDb[0]), config::METER_MIN, config::METER_MAX, meterAreaB, meterAreaY);
        auto meterHeightS = juce::jmap(juce::jlimit(config::METER_MIN, config::METER_MAX, localPath.meterLevelsDb[1]), config::METER_MIN, config::METER_MAX, meterAreaB, meterAreaY);
        g.setColour(config::theme.withAlpha(0.55f));
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(meterAreaX + meterAreaW * 0.0f, meterHeightM, meterAreaX + meterAreaW * 0.5f, meterAreaB));
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(meterAreaX + meterAreaW * 0.5f, meterHeightS, meterAreaX + meterAreaW * 1.0f, meterAreaB));

        auto peakY_M = juce::jmap(juce::jlimit(config::METER_MIN, config::METER_MAX, localPath.meterLevelsPeakDb[0]), config::METER_MIN, config::METER_MAX, meterAreaB, meterAreaY);
        auto peakY_S = juce::jmap(juce::jlimit(config::METER_MIN, config::METER_MAX, localPath.meterLevelsPeakDb[1]), config::METER_MIN, config::METER_MAX, meterAreaB, meterAreaY);
        const float peakLineThickness = 1.5f;
        g.setColour(localPath.meterLevelsPeakDb[0] > 0.0f ? config::red : config::theme);
        g.fillRect(meterAreaX + meterAreaW * 0.0f, peakY_M - peakLineThickness * 0.5f, meterAreaW * 0.5f, peakLineThickness);
        g.setColour(localPath.meterLevelsPeakDb[1] > 0.0f ? config::red : config::theme);
        g.fillRect(meterAreaX + meterAreaW * 0.5f, peakY_S - peakLineThickness * 0.5f, meterAreaW * 0.5f, peakLineThickness);

        auto spectrumArea = getCurveArea().toFloat();
        auto spectrumAreaX = spectrumArea.getX();
        auto spectrumAreaY = spectrumArea.getY();
        auto spectrumAreaW = spectrumArea.getWidth();
        auto spectrumAreaB = spectrumArea.getBottom();

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

        for (int i = 0; i < config::BAND_COUNT; ++i) {
            if (getBandParamValue(config::ID_BAND_BYPASS, i) > 0.5f) continue;
            float x = freqToX(getBandParamValue(config::ID_BAND_FREQ, i), spectrumAreaX, spectrumAreaW);
            float y = gainToY(getBandParamValue(config::ID_BAND_GAIN, i), spectrumAreaY, spectrumAreaB);
            const int pointSize = 14;
            g.setColour(config::textBackground);
            g.fillEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize);
            g.setColour(config::text);
            g.drawEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize, 1.5f);
            g.setFont(12.0f);
            drawLabel(g, juce::String(IndexToID(i)), x, y);
        }
    }

    int findNextAvailableBand() const {
        for (int i = 0; i < config::BAND_COUNT; ++i) {
            if (getBandParamValue(config::ID_BAND_BYPASS, i) > 0.5f) {
                return i;
            }
        }
        return NoBandSelected;
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

        auto& apvts = audioProcessor.apvts;
        auto setParam = [&](const juce::String& paramID, float value) {
            if (auto* p = apvts.getParameter(config::getID(paramID, availableIdx))) {
                p->setValueNotifyingHost(value);
            }
        };

        float filterMode = getFilterModeCallback ?
            apvts.getParameterRange(config::getID(config::ID_BAND_FILTER, availableIdx)).convertTo0to1((float)getFilterModeCallback()) :
            apvts.getParameter(config::getID(config::ID_BAND_FILTER, availableIdx))->getDefaultValue();
        float channelMode = getChannelModeCallback ?
            apvts.getParameterRange(config::getID(config::ID_BAND_CHANNEL, availableIdx)).convertTo0to1((float)getChannelModeCallback()) :
            apvts.getParameter(config::getID(config::ID_BAND_CHANNEL, availableIdx))->getDefaultValue();

        auto bounds = getCurveArea().toFloat();
        float freqHz = xToFreq(e.position.getX(), bounds.getX(), bounds.getWidth());
        float gainDb = yToGain(e.position.getY(), bounds.getY(), bounds.getBottom());

        setParam(config::ID_BAND_BYPASS, 0.0f);
        setParam(config::ID_BAND_FREQ, apvts.getParameterRange(config::getID(config::ID_BAND_FREQ, availableIdx)).convertTo0to1(freqHz));
        setParam(config::ID_BAND_GAIN, apvts.getParameterRange(config::getID(config::ID_BAND_GAIN, availableIdx)).convertTo0to1(gainDb));
        setParam(config::ID_BAND_QUAL, apvts.getParameter(config::getID(config::ID_BAND_QUAL, availableIdx))->getDefaultValue());
        setParam(config::ID_BAND_FILTER, filterMode);
        setParam(config::ID_BAND_CHANNEL, channelMode);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (draggingBand == NoBandSelected) return;
        auto mousePos = e.position;
        auto bounds = getCurveArea().toFloat();
        float freqHz = xToFreq(mousePos.getX(), bounds.getX(), bounds.getWidth());
        float gainDb = yToGain(mousePos.getY(), bounds.getY(), bounds.getBottom());
        auto setParam = [&](const juce::String& paramID, float plainValue) {
            juce::String index = config::getID(paramID, draggingBand);
            if (auto* p = audioProcessor.apvts.getParameter(index)) {
                p->setValueNotifyingHost(audioProcessor.apvts.getParameterRange(index).convertTo0to1(plainValue));
            }
        };
        setParam(config::ID_BAND_FREQ, freqHz);
        setParam(config::ID_BAND_GAIN, gainDb);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        draggingBand = NoBandSelected;
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (draggingBand == NoBandSelected) return;
        const juce::String paramID = config::getID(config::ID_BAND_QUAL, draggingBand);
        if (auto* param = audioProcessor.apvts.getParameter(paramID)) {
            float currentNormalized = param->getValue();
            float step = wheel.deltaY * 0.125f;
            float newNormalized = juce::jlimit(0.0f, 1.0f, currentNormalized + step);
            param->setValueNotifyingHost(newNormalized);
        }
    }
    std::function<int()> getFilterModeCallback;
    std::function<int()> getChannelModeCallback;
private:
    class AnalyzerThread: public juce::Thread
    {
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
        for (int i = 0; i < config::BAND_COUNT; ++i) {
            if (getBandParamValue(config::ID_BAND_BYPASS, i) > 0.5f) continue;
            float x = freqToX(getBandParamValue(config::ID_BAND_FREQ, i), area.getX(), area.getWidth());
            float y = gainToY(getBandParamValue(config::ID_BAND_GAIN, i), area.getY(), area.getBottom());
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
        if (parametersNeedUpdate) {
            calculateResponseCurve();
            parametersNeedUpdate = false;
        }
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
        g.drawText(text, juce::Rectangle<int>(marginSize, marginSize).withCentre({x, y}), juce::Justification::centred, false);
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
        for (auto f : config::frequencies) {
            int x = freqToX(f, curveArea.getX(), curveArea.getWidth());
            g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
            g.drawVerticalLine(x, curveArea.getY(), curveArea.getBottom());
            g.setColour(config::text);
            g.setFont(textSize);
            drawLabel(g, formatFreq(f), x, (int)curveArea.getBottom() + textSize);
            drawLabel(g, formatFreq(f), x, (int)curveArea.getY() - textSize);
        }
        for (auto db : config::editorDBs) {
            int y = gainToY(db, curveArea.getY(), curveArea.getBottom());
            g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
            g.drawHorizontalLine(y, curveArea.getX(), curveArea.getRight());
            g.setColour(config::text);
            drawLabel(g, formatDb(db), (int)curveArea.getX() - textSize, y);
        }
        g.setColour(config::text);
        for (auto db : config::fftDBs) {
            int y = juce::jmap(db, config::FFT_MIN_DB, config::FFT_MAX_DB, curveArea.getBottom(), curveArea.getY());
            drawLabel(g, formatDb(db), (int)curveArea.getRight() + textSize, y);
        }
        for (auto db : config::meterDBs) {
            int y = juce::jmap(db, config::METER_MAX, config::METER_MIN, meterArea.getY(), meterArea.getBottom());
            g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
            g.drawHorizontalLine(y, meterArea.getX(), meterArea.getRight());
            g.setColour(config::text);
            drawLabel(g, formatDb(db), (int)meterArea.getX() - textSize, y);
        }
        g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
        for (float ratio : { 0.0f, 0.5f, 1.0f }) {
            g.drawVerticalLine((int)(meterArea.getX() + meterArea.getWidth() * ratio), meterArea.getY(), meterArea.getBottom());
        }
    }

    juce::Rectangle<int> getLevelMeterArea() {
        auto a = getLocalBounds().removeFromRight(98);
        return a.reduced(marginSize).reduced(4);
    }
    juce::Rectangle<int> getCurveArea() {
        auto a = getLocalBounds();
        a.removeFromRight(98);
        return a.reduced(marginSize).reduced(4);
    }

    void calculateResponseCurve() {
        int curveSize = getCurveArea().getWidth();
        auto sampleRate = static_cast<float>(audioProcessor.getSampleRate());
        auto bounds = getCurveArea().toFloat();
        auto snapshots = audioProcessor.getFilterSnapshots();
        responseCurvePath[0].clear();
        responseCurvePath[1].clear();
        for (int i = 0; i < curveSize; ++i) {
            float normalizedX = (float)i / (float)(curveSize - 1);
            float freqHz = juce::mapToLog10(normalizedX, (float)config::PARAM_FREQ_MIN, (float)config::PARAM_FREQ_MAX);
            float magSqMid = 1.0f;
            float magSqSide = 1.0f;
            for (const auto& s : snapshots) {
                float m = std::norm(s.filter.get_response(freqHz, sampleRate));
                if (s.channelMode == 0 || s.channelMode == 1) {
                    magSqMid *= m;
                }
                if (s.channelMode == 0 || s.channelMode == 2) {
                    magSqSide *= m;
                }
            }
            auto getMagY = [&](float value) {
                return gainToY(zlth::unit::magSqToDB(value), bounds.getY(), bounds.getBottom());
            };
            float x = bounds.getX() + bounds.getWidth() * normalizedX;
            if (i == 0) {
                responseCurvePath[0].startNewSubPath(x, getMagY(magSqMid));
                responseCurvePath[1].startNewSubPath(x, getMagY(magSqSide));
            }
            else {
                responseCurvePath[0].lineTo(x, getMagY(magSqMid));
                responseCurvePath[1].lineTo(x, getMagY(magSqSide));
            }
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
            float x = freqToX(resampled[i].first, spectrumAreaX, spectrumAreaW);
            float y = juce::jmap(resampled[i].second, config::FFT_MIN_DB, config::FFT_MAX_DB, spectrumAreaB, spectrumAreaY);
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
        return audioProcessor.apvts.getRawParameterValue(config::getID(prefix, bandIdx))->load();
    }

    float freqToX(float value, float x, float w) const {
        return x + w * juce::mapFromLog10(value, config::PARAM_FREQ_MIN, config::PARAM_FREQ_MAX);
    }
    float xToFreq(float value, float x, float w) const {
        return juce::jlimit(config::PARAM_FREQ_MIN, config::PARAM_FREQ_MAX, juce::mapToLog10(juce::jlimit(0.0f, 1.0f, (value - x) / w), config::PARAM_FREQ_MIN, config::PARAM_FREQ_MAX));
    }
    float gainToY(float value, float y, float b) const {
        return juce::jmap(value, config::PARAM_GAIN_MIN, config::PARAM_GAIN_MAX, b, y);
    }
    float yToGain(float value, float y, float b) const {
        return juce::jlimit(config::PARAM_GAIN_MIN, config::PARAM_GAIN_MAX, juce::jmap(value, b, y, config::PARAM_GAIN_MIN, config::PARAM_GAIN_MAX));
    }

    static constexpr int textSize = 10;
    static constexpr int THREAD_SLEEP_TIME = 20;
    static constexpr int marginSize = textSize * 2;

    int draggingBand = NoBandSelected;
    bool parametersNeedUpdate = true;

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

    std::array<juce::Path, 2> responseCurvePath;
    static constexpr int NoBandSelected = -1;
};
