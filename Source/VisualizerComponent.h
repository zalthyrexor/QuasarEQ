#pragma once

#include <JuceHeader.h>
#include "lnf.h"
#include "PluginProcessor.h"
#include "PathProducer.h"

class VisualizerComponent: public juce::Component, private juce::AsyncUpdater, public juce::AudioProcessorValueTreeState::Listener
{
public:
    VisualizerComponent(QuasarEQAudioProcessor& p) :
        audioProcessor(p),
        pathProducer(audioProcessor.leftChannelFifo, audioProcessor.rightChannelFifo),
        analyzerThread(pathProducer, *this)
    {
        freqLUT = pathProducer.makeFreqLUT(audioProcessor.getSampleRate(), MIN_HZ, MAX_HZ);
        for (int i = 0; i < config::BAND_COUNT; ++i)
        {
            const juce::String index = juce::String(i + 1);
            for (const auto& prefix : bandParamPrefixes)
            {
                audioProcessor.apvts.addParameterListener(prefix + index, this);
            }
        }
    };
    ~VisualizerComponent()
    {
        for (int i = 0; i < config::BAND_COUNT; ++i)
        {
            const juce::String index = juce::String(i + 1);
            for (const auto& prefix : bandParamPrefixes)
            {
                audioProcessor.apvts.removeParameterListener(prefix + index, this);
            }
        }
    };
    void parameterChanged(const juce::String& parameterID, float newValue)
    {
        parametersNeedUpdate = true;
    };
    juce::Path createBezierPath(const std::vector<juce::Point<float>>& points)
    {
        juce::Path p {};
        size_t a = points.size();

        static constexpr float BEZIER_SCALE = 1.0f / 6.0f;
        p.startNewSubPath(points[0]);
        p.lineTo(points[1]);

        for (size_t i = 1; i < 33; ++i)
        {
            const auto& p0 = points[i - 1];
            const auto& p1 = points[i];
            const auto& p2 = points[i + 1];
            const auto& p3 = points[i + 2];
            const juce::Point<float> cp1 = p1 + (p2 - p0) * BEZIER_SCALE;
            const juce::Point<float> cp2 = p2 - (p3 - p1) * BEZIER_SCALE;
            p.cubicTo(cp1, cp2, p2);
        }

        for (size_t i = 32; i < a; ++i)
        {
            p.lineTo(points[i]);
        }
        return p;
    };
    void paint(juce::Graphics& g) override
    {
        g.drawImageAt(gridCache, 0, 0);
        SpectrumRenderData localPath;
        {
            juce::ScopedLock lock(pathLock);
            localPath = channelPathToDraw;
        }
        g.saveState();
        g.reduceClipRegion(getCurveArea());
        spectrumPoints.clear();
        peakHoldPoints.clear();
        spectrumPoints.reserve(localPath.spectrumPath.size());
        peakHoldPoints.reserve(localPath.spectrumPath.size());
        if (localPath.spectrumPath.size() != 0)
        {
            for (size_t i = 1; i < localPath.spectrumPath.size(); ++i)
            {
                const float freq = freqLUT[i] * getCurveArea().getWidth() + getCurveArea().getX();
                spectrumPoints.emplace_back(freq, juce::jmap(localPath.spectrumPath[i], MIN_DBFS, MAX_DBFS, getCurveArea().toFloat().getBottom(), getCurveArea().toFloat().getY()));
                peakHoldPoints.emplace_back(freq, juce::jmap(localPath.peakHoldPath[i], MIN_DBFS, MAX_DBFS, getCurveArea().toFloat().getBottom(), getCurveArea().toFloat().getY()));
            }
        }
        if (spectrumPoints.size() != 0)
        {
            juce::Path curvePathPeak = createBezierPath(spectrumPoints);
            curvePathPeak.lineTo(spectrumPoints.back().x, getCurveArea().toFloat().getBottom());
            curvePathPeak.lineTo(spectrumPoints[0].x, getCurveArea().toFloat().getBottom());
            curvePathPeak.closeSubPath();
            g.setColour(juce::Colour(zlth::ui::colors::theme).withAlpha(0.45f));
            g.fillPath(curvePathPeak);
        }
        if (peakHoldPoints.size() != 0)
        {
            juce::Path curvePathPeak = createBezierPath(peakHoldPoints);
            g.setColour(juce::Colour(zlth::ui::colors::theme));
            g.strokePath(curvePathPeak, juce::PathStrokeType(1.5f));
        }
        auto& apvts = audioProcessor.apvts;

        g.setColour(juce::Colour(zlth::ui::colors::side));
        g.strokePath(responseCurvePathSide, juce::PathStrokeType(2.0f));
        //g.setFillType(juce::FillType(juce::Colour(zlth::ui::colors::theme).withAlpha(0.25f)));
        //g.fillPath(responseCurvePathSide);

        g.setColour(juce::Colour(zlth::ui::colors::theme));
        g.strokePath(responseCurvePathMid, juce::PathStrokeType(2.0f));
        //g.setFillType(juce::FillType(juce::Colour(zlth::ui::colors::theme).withAlpha(0.25f)));
        //g.fillPath(responseCurvePathMid);


        g.restoreState();
        const float high = 12.0f;
        const float low = -36.0f;
        const float lClamped = juce::jlimit(low, high, localPath.leftDB);
        const float rClamped = juce::jlimit(low, high, localPath.rightDB);
        const int leftY = juce::roundToInt(juce::jmap(lClamped, low, high, getLevelMeterArea().toFloat().getBottom(), getLevelMeterArea().toFloat().getY()));
        const int rightY = juce::roundToInt(juce::jmap(rClamped, low, high, getLevelMeterArea().toFloat().getBottom(), getLevelMeterArea().toFloat().getY()));
        g.setColour(juce::Colour(zlth::ui::colors::theme).withAlpha(0.55f));
        g.fillRect(juce::Rectangle<int>::leftTopRightBottom(
            getLevelMeterArea().getX() + getLevelMeterArea().getWidth() * 0.25,
            leftY,
            getLevelMeterArea().getX() + getLevelMeterArea().getWidth() * 0.75,
            getLevelMeterArea().getBottom()));
        g.fillRect(juce::Rectangle<int>::leftTopRightBottom(
            getLevelMeterArea().getX(),
            rightY,
            getLevelMeterArea().getX() + getLevelMeterArea().getWidth() * 0.25,
            getLevelMeterArea().getBottom()));
        g.fillRect(juce::Rectangle<int>::leftTopRightBottom(
            getLevelMeterArea().getX() + getLevelMeterArea().getWidth() * 0.75,
            rightY,
            getLevelMeterArea().getX() + getLevelMeterArea().getWidth(),
            getLevelMeterArea().getBottom()));
        auto bounds = getCurveArea().toFloat();
        const float minDb = -24.0f;
        const float maxDb = 24.0f;
        for (int i = 0; i < config::BAND_COUNT; ++i)
        {
            juce::String index = juce::String(i + 1);
            const auto bypass = apvts.getRawParameterValue(ID_BAND_BYPASS + index)->load();
            if (bypass < 0.5f)
            {
                float freqHz = apvts.getRawParameterValue(ID_BAND_FREQ + index)->load();
                float gainDb = apvts.getRawParameterValue(ID_BAND_GAIN + index)->load();
                float x = bounds.getX() + bounds.getWidth() * juce::mapFromLog10(freqHz, MIN_HZ, MAX_HZ);
                float y = juce::jmap(gainDb, minDb, maxDb, bounds.getBottom(), bounds.getY());
                g.setColour(juce::Colour(zlth::ui::colors::textBackground));
                const int pointSize = 14;
                const int highLightPointSize = pointSize * 2;
                g.fillEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize);
                g.setColour(juce::Colour(zlth::ui::colors::text));
                g.drawEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize, 1.5f);
                juce::String bandNumber = juce::String(i + 1);
                const int textHeight = 12;
                g.setFont(textHeight);
                const int textWidth = g.getCurrentFont().getStringWidth(bandNumber);
                juce::Rectangle<int> textBounds(juce::roundToInt(x - textWidth * 0.5f), y - 6, textWidth, textHeight);
                g.drawText(bandNumber, textBounds, juce::Justification::centred, false);
            }
        }
    };
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto bounds = getCurveArea();
        if (!bounds.contains(e.getPosition()))
        {
            draggingBand = -1;
            return;
        }
        draggingBand = getClosestBand(e.position);
        if (draggingBand == -1)
        {
            auto& apvts = audioProcessor.apvts;
            for (int i = 0; i < config::BAND_COUNT; ++i)
            {
                juce::String index = juce::String(i + 1);
                bool isBypassed = apvts.getRawParameterValue(ID_BAND_BYPASS + index)->load() > 0.5f;
                if (isBypassed)
                {
                    draggingBand = i;
                    auto bounds = getCurveArea().toFloat();
                    const float minDb = -24.0f;
                    const float maxDb = 24.0f;
                    float normalizedX = (e.position.getX() - bounds.getX()) / bounds.getWidth();
                    float freqHz = juce::mapToLog10(juce::jlimit(0.0f, 1.0f, normalizedX), MIN_HZ, MAX_HZ);
                    float gainDb = juce::jmap(e.position.getY(), bounds.getBottom(), bounds.getY(), minDb, maxDb);
                    gainDb = juce::jlimit(minDb, maxDb, gainDb);
                    if (auto* bpParam = apvts.getParameter(ID_BAND_BYPASS + index))
                        bpParam->setValueNotifyingHost(0.0f);
                    if (auto* fParam = apvts.getParameter(ID_BAND_FREQ + index))
                        fParam->setValueNotifyingHost(apvts.getParameterRange(ID_BAND_FREQ + index).convertTo0to1(freqHz));
                    if (auto* gParam = apvts.getParameter(ID_BAND_GAIN + index))
                        gParam->setValueNotifyingHost(apvts.getParameterRange(ID_BAND_GAIN + index).convertTo0to1(gainDb));
                    if (auto* qParam = apvts.getParameter(ID_BAND_QUAL + index))
                        qParam->setValueNotifyingHost(qParam->getDefaultValue());
                    if (auto* cParam = apvts.getParameter(ID_BAND_CHANNEL + index))
                        cParam->setValueNotifyingHost(cParam->getDefaultValue());
                    if (auto* tParam = apvts.getParameter(ID_BAND_FILTER + index))
                    {
                        float normalizedValue = 0.0f;
                        if (getSelectedTypeCallback)
                        {
                            normalizedValue = apvts.getParameterRange(ID_BAND_FILTER + index).convertTo0to1((float)getSelectedTypeCallback());
                        }
                        else
                        {
                            normalizedValue = tParam->getDefaultValue();
                        }
                        tParam->setValueNotifyingHost(normalizedValue);
                    }
                    break;
                }
            }
        }
        if (draggingBand != -1)
        {
            juce::String index = juce::String(draggingBand + 1);
            DBG("--- GESTURE START ---");
            if (auto* p1 = audioProcessor.apvts.getParameter(ID_BAND_FREQ + index)) p1->beginChangeGesture();
            if (auto* p2 = audioProcessor.apvts.getParameter(ID_BAND_GAIN + index)) p2->beginChangeGesture();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggingBand != -1)
        {
            updateParamsFromMouse(e.position);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (draggingBand != -1)
        {
            juce::String index = juce::String(draggingBand + 1);
            DBG("--- GESTURE END ---");
            if (auto* p1 = audioProcessor.apvts.getParameter(ID_BAND_FREQ + index)) p1->endChangeGesture();
            if (auto* p2 = audioProcessor.apvts.getParameter(ID_BAND_GAIN + index)) p2->endChangeGesture();
            draggingBand = -1;
        }
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const int bandIdx = getClosestBand(e.position);
        if (bandIdx == -1) return;
        const juce::String paramID = ID_BAND_QUAL + juce::String(bandIdx + 1);
        if (auto* qParam = audioProcessor.apvts.getParameter(paramID))
        {
            float currentRealValue = audioProcessor.apvts.getRawParameterValue(paramID)->load();
            float currentNormalized = qParam->getValue();
            float newNormalized = juce::jlimit(0.0f, 1.0f, currentNormalized + (wheel.deltaY * 0.1f));
            auto range = audioProcessor.apvts.getParameterRange(paramID);
            float newRealValue = range.snapToLegalValue(range.convertFrom0to1(newNormalized));
            if (newRealValue == currentRealValue && wheel.deltaY != 0)
            {
                newRealValue += (wheel.deltaY > 0) ? range.interval : -range.interval;
            }
            qParam->setValueNotifyingHost(range.convertTo0to1(juce::jlimit(range.start, range.end, newRealValue)));
        }
    }
    std::function<int()> getSelectedTypeCallback;
private:
    class AnalyzerThread: public juce::Thread
    {
    public:
        AnalyzerThread(PathProducer& producer, VisualizerComponent& comp) : juce::Thread("FFT Analyzer Thread"), producer(producer), responseCurveComponent(comp)
        {
            startThread();
        };
        ~AnalyzerThread() override
        {
            stopThread(1000);
        };
        void run() override
        {
            while (!threadShouldExit())
            {
                producer.process(responseCurveComponent.audioProcessor.getSampleRate());
                if (!responseCurveComponent.isUpdatePending() && producer.getNumPathsAvailable() > 0)
                {
                    responseCurveComponent.triggerAsyncUpdate();
                }
                juce::Thread::sleep(THREAD_SLEEP_TIME);
            }
        };
    private:
        PathProducer& producer;
        VisualizerComponent& responseCurveComponent;
    };
    int getClosestBand(juce::Point<float> mousePos)
    {
        auto bounds = getCurveArea().toFloat();
        const float xBase = bounds.getX();
        const float width = bounds.getWidth();
        const float yBottom = bounds.getBottom();
        const float yTop = bounds.getY();
        const float minDb = EDITOR_MIN_DBFS;
        const float maxDb = EDITOR_MAX_DBFS;
        const float toleranceRadius = 12.0f;
        const float thresholdSq = toleranceRadius * toleranceRadius;
        auto& apvts = audioProcessor.apvts;
        for (int i = 0; i < config::BAND_COUNT; ++i)
        {
            juce::String index = juce::String(i + 1);
            if (apvts.getRawParameterValue(ID_BAND_BYPASS + index)->load() > 0.5f)
                continue;
            float freqHz = apvts.getRawParameterValue(ID_BAND_FREQ + index)->load();
            float gainDb = apvts.getRawParameterValue(ID_BAND_GAIN + index)->load();
            float x = xBase + width * juce::mapFromLog10(freqHz, MIN_HZ, MAX_HZ);
            float y = juce::jmap(gainDb, minDb, maxDb, yBottom, yTop);
            if (mousePos.getDistanceSquaredFrom({x, y}) < thresholdSq)
                return i;
        }
        return -1;
    }
    void updateParamsFromMouse(juce::Point<float> mousePos)
    {
        if (draggingBand == -1) return;
        auto bounds = getCurveArea().toFloat();
        const float minDb = EDITOR_MIN_DBFS;
        const float maxDb = EDITOR_MAX_DBFS;
        float normalizedX = (mousePos.getX() - bounds.getX()) / bounds.getWidth();
        float freqHz = juce::mapToLog10(juce::jlimit(0.0f, 1.0f, normalizedX), MIN_HZ, MAX_HZ);
        float gainDb = juce::jmap(mousePos.getY(), bounds.getBottom(), bounds.getY(), minDb, maxDb);
        gainDb = juce::jlimit(minDb, maxDb, gainDb);
        freqHz = juce::jlimit(MIN_HZ, MAX_HZ, freqHz);
        juce::String index = juce::String(draggingBand + 1);
        if (auto* freqParam = audioProcessor.apvts.getParameter(ID_BAND_FREQ + index))
        {
            freqParam->setValueNotifyingHost(audioProcessor.apvts.getParameterRange(ID_BAND_FREQ + index).convertTo0to1(freqHz));
        }
        if (auto* gainParam = audioProcessor.apvts.getParameter(ID_BAND_GAIN + index))
        {
            gainParam->setValueNotifyingHost(audioProcessor.apvts.getParameterRange(ID_BAND_GAIN + index).convertTo0to1(gainDb));
        }
    }
    void handleAsyncUpdate() override
    {
        SpectrumRenderData path;
        bool newPathAvailable = false;
        while (pathProducer.getNumPathsAvailable() > 0)
        {
            if (pathProducer.getPath(path))
            {
                newPathAvailable = true;
            }
        }
        if (parametersNeedUpdate)
        {
            calculateResponseCurve();
            parametersNeedUpdate = false;
        }
        if (newPathAvailable)
        {
            {
                juce::ScopedLock lock(pathLock);
                channelPathToDraw = path;
            }
            repaint();
        }
    };
    juce::Image gridCache;
    static constexpr int labelBorderSize = 48;
    static constexpr float METER_MAX = 12.0f;
    static constexpr float METER_MIN = -36.0f;
    static constexpr float MIN_HZ = 20.0f;
    static constexpr float MAX_HZ = 20000.0f;
    static constexpr float EDITOR_MIN_DBFS = -24.0f;
    static constexpr float EDITOR_MAX_DBFS = 24.0f;
    const std::vector<float> editorDBs = {24.0f, 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f};
    const std::vector<float> meterDBs = {12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f};
    const std::vector<float> frequencies = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
    void resized() override
    {
        struct Marker { juce::String text; int pos; };
        const auto curveArea = getCurveArea();
        const auto meterArea = getLevelMeterArea();
        const auto curveAreaF = curveArea.toFloat();
        const auto meterAreaF = meterArea.toFloat();
        auto formatFreqText = [](float f) { return (f < 1000.0f) ? juce::String(static_cast<int>(f)) : juce::String(static_cast<int>(f / 1000.0f)) + "k"; };
        auto formatDbText = [](float db) { return (db > 0 ? "+" : "") + juce::String(static_cast<int> (db)); };
        auto dbToY = [&](float db, const juce::Rectangle<int>& area, float maxDb, float minDb)
            {
                return (int)area.getRelativePoint(0.0f, juce::jmap(db, maxDb, minDb, 0.0f, 1.0f)).y;
            };
        auto createYMarkers = [&](const std::vector<float>& dbs, const juce::Rectangle<int>& area, float maxDb, float minDb)
            {
                std::vector<Marker> markers;
                markers.reserve(dbs.size());
                std::transform(dbs.begin(), dbs.end(), std::back_inserter(markers), [&](float db)
                    {
                        return Marker {formatDbText(db), dbToY(db, area, maxDb, minDb)};
                    });
                return markers;
            };
        std::vector<Marker> xMarkers;
        xMarkers.reserve(frequencies.size());
        std::transform(frequencies.begin(), frequencies.end(), std::back_inserter(xMarkers), [&](float f)
            {
                return Marker {formatFreqText(f), (int)curveArea.getRelativePoint(juce::mapFromLog10(f, MIN_HZ, MAX_HZ), 0.0f).x};
            });
        auto curveYMarkers = createYMarkers(editorDBs, curveArea, EDITOR_MAX_DBFS, EDITOR_MIN_DBFS);
        auto meterYMarkers = createYMarkers(meterDBs, meterArea, METER_MAX, METER_MIN);
        gridCache = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics g(gridCache);
        g.setColour(juce::Colours::black);
        g.fillRect(curveArea);
        g.fillRect(meterArea);
        g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));

        g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
        g.drawHorizontalLine(dbToY(0.0f, meterArea, METER_MAX, METER_MIN), meterAreaF.getX(), meterAreaF.getRight());
        g.drawHorizontalLine(meterAreaF.getBottom(), meterAreaF.getX(), meterAreaF.getRight());
        g.drawHorizontalLine(meterAreaF.getY(), meterAreaF.getX(), meterAreaF.getRight());
        g.drawVerticalLine(meterAreaF.getX(), meterAreaF.getY(), meterAreaF.getBottom());
        g.drawVerticalLine(meterAreaF.getX() + meterAreaF.getWidth() * 0.25, meterAreaF.getY(), meterAreaF.getBottom());
        g.drawVerticalLine(meterAreaF.getX() + meterAreaF.getWidth() * 0.75, meterAreaF.getY(), meterAreaF.getBottom());
        g.drawVerticalLine(meterAreaF.getX() + meterAreaF.getWidth(), meterAreaF.getY(), meterAreaF.getBottom());
        for (const auto& m : xMarkers)g.drawVerticalLine(m.pos, curveAreaF.getY(), curveAreaF.getBottom());
        for (const auto& m : curveYMarkers)g.drawHorizontalLine(m.pos, curveAreaF.getX(), curveAreaF.getRight());
        g.setColour(juce::Colour(zlth::ui::colors::text));
        g.setFont(FONT_HEIGHT);
        for (const auto& m : xMarkers)drawLabelAt(g, m.text, m.pos, curveArea.getBottom() + margin, labelBorderSize);
        for (const auto& m : curveYMarkers)drawLabelAt(g, m.text, curveArea.getX() - margin, m.pos, labelBorderSize);
        for (const auto& m : meterYMarkers)drawLabelAt(g, m.text, meterArea.getX() - margin, m.pos, labelBorderSize);
    }
    void drawLabelAt(juce::Graphics& g, const juce::String& text, int centreX, int centreY, int size)
    {
        g.drawText(text, juce::Rectangle<int>(size, size).withCentre({centreX, centreY}), juce::Justification::centred);
    }
    juce::Rectangle<int> getLevelMeterArea()
    {
        auto a = getLocalBounds().reduced(margin << 1);
        return a.removeFromRight(margin << 1);
    }
    juce::Rectangle<int> getCurveArea()
    {
        auto a = getLocalBounds().reduced(margin << 1);
        a.removeFromRight(margin * 6);
        return a;
    }

    void calculateResponseCurve()
    {
        int curveSize = getCurveArea().getWidth();
        double sampleRate = audioProcessor.getSampleRate();
        auto bounds = getCurveArea().toFloat();
        auto snapshots = audioProcessor.getFilterSnapshots();
        responseCurvePathMid.clear();
        responseCurvePathSide.clear();
        for (int i = 0; i < curveSize; ++i)
        {
            float normalizedX = (float)i / (float)(curveSize - 1);
            float freqHz = juce::mapToLog10(normalizedX, (float)MIN_HZ, (float)MAX_HZ);
            float magMid = 1.0f;
            float magSide = 1.0f;

            for (const auto& s : snapshots)
            {
                float m = s.filter.get_magnitude(freqHz, (float)sampleRate);
                if (s.channelMode == 0 || s.channelMode == 1) magMid *= m;
                if (s.channelMode == 0 || s.channelMode == 2) magSide *= m;
            }
            auto getPointY = [&](float gain) {
                float db = juce::Decibels::gainToDecibels(gain);
                return juce::jmap(db, (float)EDITOR_MIN_DBFS, (float)EDITOR_MAX_DBFS, bounds.getBottom(), bounds.getY());
            };
            float x = bounds.getX() + bounds.getWidth() * normalizedX;
            if (i == 0) {
                responseCurvePathMid.startNewSubPath(x, getPointY(magMid));
                responseCurvePathSide.startNewSubPath(x, getPointY(magSide));
            }
            else {
                responseCurvePathMid.lineTo(x, getPointY(magMid));
                responseCurvePathSide.lineTo(x, getPointY(magSide));
            }
        }
    }
    int draggingBand = -1;
    bool parametersNeedUpdate = true;
    static constexpr float MIN_DBFS = -90.0f;
    static constexpr float MAX_DBFS = 30.0f;
    static constexpr int HALF_FONT_HEIGHT = 5;
    static constexpr int FONT_HEIGHT = HALF_FONT_HEIGHT * 2;
    static constexpr int margin = 10;
    static constexpr int THREAD_SLEEP_TIME = 25;
    QuasarEQAudioProcessor& audioProcessor;
    PathProducer pathProducer;
    SpectrumRenderData channelPathToDraw;
    std::vector<juce::Point<float>> spectrumPoints;
    std::vector<juce::Point<float>> peakHoldPoints;
    std::vector<float> freqLUT;
    juce::CriticalSection pathLock;
    juce::Path responseCurvePathMid;
    juce::Path responseCurvePathSide;
    AnalyzerThread analyzerThread;
    std::vector<float> responseCurveMagnitude;
};
