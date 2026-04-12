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
                audioProcessor.apvts.removeParameterListener(config::getID(prefix,i), this);
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

        g.setColour(config::theme.withAlpha(0.55f));
        auto meterArea = getLevelMeterArea().toFloat();
        auto meterAreaX = meterArea.getX();
        auto meterAreaY = meterArea.getY();
        auto meterAreaW = meterArea.getWidth();
        auto meterAreaB = meterArea.getBottom();
        auto meterHeightM = juce::jmap(juce::jlimit(config::METER_MIN, config::METER_MAX, localPath.db[0]), config::METER_MIN, config::METER_MAX, meterAreaB, meterAreaY);
        auto meterHeightS = juce::jmap(juce::jlimit(config::METER_MIN, config::METER_MAX, localPath.db[1]), config::METER_MIN, config::METER_MAX, meterAreaB, meterAreaY);
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(meterAreaX + meterAreaW * 0.00, meterHeightS, meterAreaX + meterAreaW * 0.25, meterAreaB));
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(meterAreaX + meterAreaW * 0.25, meterHeightM, meterAreaX + meterAreaW * 0.75, meterAreaB));
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(meterAreaX + meterAreaW * 0.75, meterHeightS, meterAreaX + meterAreaW * 1.00, meterAreaB));

        auto spectrumArea = getCurveArea().toFloat();
        auto spectrumAreaX = spectrumArea.getX();
        auto spectrumAreaY = spectrumArea.getY();
        auto spectrumAreaW = spectrumArea.getWidth();
        auto spectrumAreaB = spectrumArea.getBottom();

        g.saveState();
        g.reduceClipRegion(getCurveArea());
        auto updatePath = [&](const auto& sourcePath, std::vector<juce::Point<float>>& targetPoints, juce::Path& targetPath, bool shouldClosePath) {
            targetPoints.clear();
            targetPath.clear();
            if (sourcePath.size() != 2048) return;
            targetPoints.reserve(sourcePath.size());
            auto resampled = resampler.resample(sourcePath.data(), audioProcessor.getSampleRate());
            for (int i = 1; i < resampled.size(); ++i) {
                float x = juce::mapFromLog10(resampled[i].first, EDITOR_MIN_HZ, EDITOR_MAX_HZ) * spectrumAreaW + spectrumAreaX;
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
        };
        updatePath(localPath.spectrumPath, spectrumPoints, spectrumPath, true);
        updatePath(localPath.peakHoldPath, peakHoldPoints, peakHoldPath, false);

        g.setColour(config::theme.withAlpha(0.45f));
        g.fillPath(spectrumPath);
        g.setColour(config::theme);
        g.strokePath(peakHoldPath, juce::PathStrokeType(1.5f));
        g.setColour(config::side);
        g.strokePath(responseCurvePathSide, juce::PathStrokeType(2.5f));
        g.setColour(config::theme);
        g.strokePath(responseCurvePathMid, juce::PathStrokeType(2.5f));
        g.restoreState();

        auto& apvts = audioProcessor.apvts;
        const float minDb = config::PARAM_BAND_GAIN_MIN;
        const float maxDb = config::PARAM_BAND_GAIN_MAX;
        const float bandCount = config::BAND_COUNT;
        for (int i = 0; i < bandCount; ++i) {
            juce::String index = juce::String(IndexToID(i));
            auto getParam = [&](const juce::String& prefix) {
                return apvts.getRawParameterValue(prefix + index)->load();
            };
            if (getParam(config::ID_BAND_BYPASS) > 0.5f) continue;
            float freqHz = getParam(config::ID_BAND_FREQ);
            float gainDb = getParam(config::ID_BAND_GAIN);
            float x = spectrumAreaX + spectrumAreaW * juce::mapFromLog10(freqHz, EDITOR_MIN_HZ, EDITOR_MAX_HZ);
            float y = juce::jmap(gainDb, minDb, maxDb, spectrumAreaB, spectrumAreaY);
            const int pointSize = 14;
            g.setColour(config::textBackground);
            g.fillEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize);
            g.setColour(config::text);
            g.drawEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize, 1.5f);
            const int textHeight = 12;
            g.setFont(textHeight);
            const int textWidth = g.getCurrentFont().getStringWidth(index);
            juce::Rectangle<int> textBounds(juce::roundToInt(x - textWidth * 0.5f), y - 6, textWidth, textHeight);
            g.drawText(index, textBounds, juce::Justification::centred, false);
        }
    }

    int findNextAvailableBand() const {
        for (int i = 0; i < config::BAND_COUNT; ++i) {
            if (audioProcessor.apvts.getRawParameterValue(config::getID(config::ID_BAND_BYPASS, i))->load() > 0.5f) {
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

        juce::String index = juce::String(IndexToID(availableIdx));
        draggingBand = availableIdx;
        auto& apvts = audioProcessor.apvts;
        auto setParam = [&](const juce::String& paramID, float value) {
            if (auto* p = apvts.getParameter(paramID + index)) {
                p->setValueNotifyingHost(value);
            }
        };
        auto bounds = getCurveArea().toFloat();
        const float minDb = -24.0f;
        const float maxDb = 24.0f;
        float freqHz = juce::mapToLog10(juce::jlimit(0.0f, 1.0f, (e.position.getX() - bounds.getX()) / bounds.getWidth()), EDITOR_MIN_HZ, EDITOR_MAX_HZ);
        float gainDb = juce::jlimit(minDb, maxDb, juce::jmap(e.position.getY(), bounds.getBottom(), bounds.getY(), minDb, maxDb));
        float normalizedValue = getSelectedTypeCallback ?
            apvts.getParameterRange(config::ID_BAND_FILTER + index).convertTo0to1((float)getSelectedTypeCallback()) :
            apvts.getParameter(config::ID_BAND_FILTER + index)->getDefaultValue();

        float aaa = getMSTypeCallback ?
            apvts.getParameterRange(config::ID_BAND_CHANNEL + index).convertTo0to1((float)getMSTypeCallback()) :
            apvts.getParameter(config::ID_BAND_CHANNEL + index)->getDefaultValue();

        setParam(config::ID_BAND_BYPASS, 0.0f);
        setParam(config::ID_BAND_FREQ, apvts.getParameterRange(config::ID_BAND_FREQ + index).convertTo0to1(freqHz));
        setParam(config::ID_BAND_GAIN, apvts.getParameterRange(config::ID_BAND_GAIN + index).convertTo0to1(gainDb));
        setParam(config::ID_BAND_QUAL, apvts.getParameter(config::ID_BAND_QUAL + index)->getDefaultValue());

        setParam(config::ID_BAND_CHANNEL, aaa);

        setParam(config::ID_BAND_FILTER, normalizedValue);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (draggingBand == NoBandSelected) return;
        auto mousePos = e.position;
        auto bounds = getCurveArea().toFloat();
        const float MIN_DB = EDITOR_MIN_DB;
        const float MAX_DB = EDITOR_MAX_DB;
        const float MIN_HZ = EDITOR_MIN_HZ;
        const float MAX_HZ = EDITOR_MAX_HZ;
        float freqHz = juce::jlimit(MIN_HZ, MAX_HZ, juce::mapToLog10(juce::jlimit(0.0f, 1.0f, (mousePos.getX() - bounds.getX()) / bounds.getWidth()), MIN_HZ, MAX_HZ));
        float gainDb = juce::jlimit(MIN_DB, MAX_DB, juce::jmap(mousePos.getY(), bounds.getBottom(), bounds.getY(), MIN_DB, MAX_DB));
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
        const int bandIdx = getClosestBand(e.position);
        if (bandIdx == NoBandSelected) return;
        const juce::String paramID = config::getID(config::ID_BAND_QUAL, bandIdx);
        if (auto* qParam = audioProcessor.apvts.getParameter(paramID)) {
            float currentRealValue = audioProcessor.apvts.getRawParameterValue(paramID)->load();
            float currentNormalized = qParam->getValue();
            float newNormalized = juce::jlimit(0.0f, 1.0f, currentNormalized + (wheel.deltaY * 0.1f));
            auto range = audioProcessor.apvts.getParameterRange(paramID);
            float newRealValue = range.snapToLegalValue(range.convertFrom0to1(newNormalized));
            if (newRealValue == currentRealValue && wheel.deltaY != 0) {
                newRealValue += (wheel.deltaY > 0) ? range.interval : -range.interval;
            }
            qParam->setValueNotifyingHost(range.convertTo0to1(juce::jlimit(range.start, range.end, newRealValue)));
        }
    }
    std::function<int()> getSelectedTypeCallback;
    std::function<int()> getMSTypeCallback;
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
        const float areaX = area.getX();
        const float areaY = area.getY();
        const float areaW = area.getWidth();
        const float areaB = area.getBottom();
        const float minDb = config::PARAM_BAND_GAIN_MIN;
        const float maxDb = config::PARAM_BAND_GAIN_MAX;
        const float toleranceRadius = 12.0f;
        const float thresholdSq = toleranceRadius * toleranceRadius;
        auto& apvts = audioProcessor.apvts;
        for (int i = 0; i < config::BAND_COUNT; ++i) {
            auto getParam = [&](const juce::String& prefix) {
                return apvts.getRawParameterValue(config::getID(prefix,i))->load();
            };
            if (getParam(config::ID_BAND_BYPASS) > 0.5f) continue;
            float freqHz = getParam(config::ID_BAND_FREQ);
            float gainDb = getParam(config::ID_BAND_GAIN);
            float x = areaX + areaW * juce::mapFromLog10(freqHz, EDITOR_MIN_HZ, EDITOR_MAX_HZ);
            float y = juce::jmap(gainDb, minDb, maxDb, areaB, areaY);
            if (mousePos.getDistanceSquaredFrom({x, y}) < thresholdSq) return i;
        }
        return NoBandSelected;
    }

    void handleAsyncUpdate() override {
        SpectrumRenderData path;
        bool newPathAvailable = false;
        while (pathProducer.getNumPathsAvailable() > 0) {
            if (pathProducer.getPath(path)) {
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
                channelPathToDraw = path;
            }
            repaint();
        }
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
        auto drawLabel = [&](const juce::String& text, int x, int y) {
            g.drawText(text, juce::Rectangle<int>(labelBorderSize, labelBorderSize).withCentre({x, y}), juce::Justification::centred);
        };
        for (auto f : frequencies) {
            int x = curveArea.getX() + juce::mapFromLog10(f, EDITOR_MIN_HZ, EDITOR_MAX_HZ) * curveArea.getWidth();
            g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
            g.drawVerticalLine(x, curveArea.getY(), curveArea.getBottom());
            g.setColour(config::text);
            g.setFont(margin);
            drawLabel(formatFreq(f), x, (int)curveArea.getBottom() + margin);
            drawLabel(formatFreq(f), x, (int)curveArea.getY() - margin);
        }
        for (auto db : editorDBs) {
            int y = juce::jmap(db, EDITOR_MAX_DB, EDITOR_MIN_DB, curveArea.getY(), curveArea.getBottom());
            g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
            g.drawHorizontalLine(y, curveArea.getX(), curveArea.getRight());
            g.setColour(config::text);
            drawLabel(formatDb(db), (int)curveArea.getX() - margin, y);
        }
        for (auto db : meterDBs) {
            int y = juce::jmap(db, config::METER_MAX, config::METER_MIN, meterArea.getY(), meterArea.getBottom());
            g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
            g.drawHorizontalLine(y, meterArea.getX(), meterArea.getRight());
            g.setColour(config::text);
            drawLabel(formatDb(db), (int)meterArea.getX() - margin, y);
        }
        g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
        for (float ratio : { 0.0f, 0.25f, 0.75f, 1.0f }) {
            g.drawVerticalLine((int)(meterArea.getX() + meterArea.getWidth() * ratio), meterArea.getY(), meterArea.getBottom());
        }
    }

    juce::Rectangle<int> getLevelMeterArea() {
        auto a = getLocalBounds().removeFromRight(86);
        return a.reduced(margin * 2).reduced(4);
    }
    juce::Rectangle<int> getCurveArea() {
        auto a = getLocalBounds();
        a.removeFromRight(86);
        return a.reduced(margin * 2).reduced(4);
    }

    void calculateResponseCurve() {
        int curveSize = getCurveArea().getWidth();
        double sampleRate = audioProcessor.getSampleRate();
        auto bounds = getCurveArea().toFloat();
        auto snapshots = audioProcessor.getFilterSnapshots();
        responseCurvePathMid.clear();
        responseCurvePathSide.clear();
        for (int i = 0; i < curveSize; ++i) {
            float normalizedX = (float)i / (float)(curveSize - 1);
            float freqHz = juce::mapToLog10(normalizedX, (float)EDITOR_MIN_HZ, (float)EDITOR_MAX_HZ);
            float magSqMid = 1.0f;
            float magSqSide = 1.0f;
            for (const auto& s : snapshots) {
                auto res = s.filter.get_response(freqHz, (float)sampleRate);
                float m = std::norm(res);
                if (s.channelMode == 0 || s.channelMode == 1) {
                    magSqMid *= m;
                }
                if (s.channelMode == 0 || s.channelMode == 2) {
                    magSqSide *= m;
                }
            }
            auto getMagY = [&](float magSq) {
                float db = 10.0f * std::log10(std::max(magSq, 1e-10f));
                return juce::jmap(db, (float)EDITOR_MIN_DB, (float)EDITOR_MAX_DB, bounds.getBottom(), bounds.getY());
            };
            float x = bounds.getX() + bounds.getWidth() * normalizedX;
            if (i == 0) {
                responseCurvePathMid.startNewSubPath(x, getMagY(magSqMid));
                responseCurvePathSide.startNewSubPath(x, getMagY(magSqSide));
            }
            else {
                responseCurvePathMid.lineTo(x, getMagY(magSqMid));
                responseCurvePathSide.lineTo(x, getMagY(magSqSide));
            }
        }
    }

    static constexpr int margin = 10;
    static constexpr int THREAD_SLEEP_TIME = 20;
    static constexpr int labelBorderSize = 48;

    static constexpr float EDITOR_MIN_HZ = 20.0f;
    static constexpr float EDITOR_MAX_HZ = 20000.0f;

    static constexpr float EDITOR_MIN_DB = -24.0f;
    static constexpr float EDITOR_MAX_DB = 24.0f;

    int draggingBand = NoBandSelected;
    bool parametersNeedUpdate = true;

    const std::vector<float> editorDBs = {24.0f, 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f};
    const std::vector<float> meterDBs = {12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f};
    const std::vector<float> frequencies = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};

    QuasarEQAudioProcessor& audioProcessor;
    PathProducer pathProducer;
    SpectrumRenderData channelPathToDraw;
    AnalyzerThread analyzerThread;
    juce::Image gridCache;
    juce::CriticalSection pathLock;
    zlth::dsp::fft::Resampler510 resampler;

    std::vector<juce::Point<float>> spectrumPoints;
    std::vector<juce::Point<float>> peakHoldPoints;

    juce::Path spectrumPath;
    juce::Path peakHoldPath;
    juce::Path responseCurvePathMid;
    juce::Path responseCurvePathSide;
    static constexpr int NoBandSelected = -1;
};
