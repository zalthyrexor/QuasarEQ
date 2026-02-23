#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <vector>
#include "zlth_fifo.h"
#include "zlth_dsp_fft.h"
#include "zlth_dsp_window.h"
#include "zlth_dsp_window_coefficients.h"
#include "zlth_simd.h"
#include "lnf.h"

struct SpectrumRenderData
{
    std::vector<float> spectrumPath;
    std::vector<float> peakHoldPath;
    float leftDB = -100.0f;
    float rightDB = -100.0f;
};

class PathProducer
{
public:
    PathProducer(SingleChannelSampleFifo& leftScsf, SingleChannelSampleFifo& rightScsf): channelFifoL(&leftScsf), channelFifoR(&rightScsf)
    {
        fftBuffer.setSize(1, FFT_SIZE, false, true, true);
        monoBufferL.setSize(1, FFT_SIZE, false, true, true);
        monoBufferR.setSize(1, FFT_SIZE, false, true, true);
        decibelsPeak.assign(FFT_SIZE_HALF, -100.0f);
        gainsBuffer.assign(FFT_SIZE_HALF, 0.0f);
        windowTable.assign(FFT_SIZE, 0.0f);
        decibelsCurrent.assign(FFT_SIZE_HALF, 0.0f);
        zlth::dsp::window::fill_window(windowTable, zlth::dsp::window::coefficients::blackman_harris_92);
        zlth::simd::multiply(windowTable, windowTable.size() / std::accumulate(windowTable.begin(), windowTable.end(), 0.0));
        for (int i = 0; i < 32; ++i)
        {
            auto& data = pathFifo.getBufferAt(i);
            data.spectrumPath.assign(FFT_SIZE_HALF, -100.0f);
            data.peakHoldPath.assign(FFT_SIZE_HALF, -100.0f);
        }
    }
    void process(double sampleRate)
    {
        juce::AudioBuffer<float> incomingBufferL, incomingBufferR;
        while (channelFifoL->getNumCompleteBuffersAvailable() > 0 && channelFifoR->getNumCompleteBuffersAvailable() > 0)
        {
            if (channelFifoL->getAudioBuffer(incomingBufferL) && channelFifoR->getAudioBuffer(incomingBufferR))
            {
                const int originalIncomingSize = incomingBufferL.getNumSamples();
                decibelLCurrent = juce::Decibels::gainToDecibels(incomingBufferL.getMagnitude(0, 0, originalIncomingSize));
                decibelRCurrent = juce::Decibels::gainToDecibels(incomingBufferR.getMagnitude(0, 0, originalIncomingSize));
                const int useSize = std::min(originalIncomingSize, FFT_SIZE);
                const int sourceOffset = originalIncomingSize - useSize;

                const int copySize = FFT_SIZE - useSize;

                if (copySize > 0)
                {
                    monoBufferL.copyFrom(0, 0, monoBufferL.getReadPointer(0, useSize), copySize);
                    monoBufferR.copyFrom(0, 0, monoBufferR.getReadPointer(0, useSize), copySize);
                }

                monoBufferL.copyFrom(0, copySize, incomingBufferL.getReadPointer(0, sourceOffset), useSize);
                monoBufferR.copyFrom(0, copySize, incomingBufferR.getReadPointer(0, sourceOffset), useSize);

                auto destSpan = std::span(fftBuffer.getWritePointer(0), static_cast<size_t>(fftBuffer.getNumSamples()));
                auto s1Span = std::span(monoBufferL.getReadPointer(0), static_cast<size_t>(monoBufferL.getNumSamples()));
                auto s2Span = std::span(monoBufferR.getReadPointer(0), static_cast<size_t>(monoBufferR.getNumSamples()));
                zlth::simd::average_two_buffers(destSpan, s1Span, s2Span);
                zlth::simd::multiply_two_buffers(destSpan, std::span(windowTable));
                auto fullDestSpan = std::span(fftBuffer.getWritePointer(0), static_cast<size_t>(fftBuffer.getNumSamples()));
                auto magnitudesSpan = fft.performFFT(fullDestSpan.first<FFT_SIZE>());
                const float dt = static_cast<float>(originalIncomingSize) / sampleRate;
                const float peakFall = 15.0f * dt;
                const float meterFall = 50 * dt;
                const float releaseSpeedFactor = 1.0f - std::exp(-dt * 50);
                for (size_t i = 0; i < gainsBuffer.size(); ++i)
                {
                    const float target = magnitudesSpan[i];
                    if (target > gainsBuffer[i])
                    {
                        gainsBuffer[i] = target;
                    }
                    else
                    {
                        gainsBuffer[i] += releaseSpeedFactor * (target - gainsBuffer[i]);
                    }
                }
                zlth::simd::gains_to_decibels(std::span(decibelsCurrent), gainsBuffer, -100.0f);
                zlth::simd::apply_falloff(std::span(decibelsPeak), std::span(decibelsCurrent), peakFall);
                decibelLSmoothed = juce::jmax(decibelLCurrent, decibelLSmoothed - meterFall);
                decibelRSmoothed = juce::jmax(decibelRCurrent, decibelRSmoothed - meterFall);
            }
        }
        if (auto* renderData = pathFifo.getWriteBuffer())
        {
            std::copy(decibelsCurrent.begin(), decibelsCurrent.end(), renderData->spectrumPath.begin());
            std::copy(decibelsPeak.begin(), decibelsPeak.end(), renderData->peakHoldPath.begin());
            renderData->leftDB = decibelLSmoothed;
            renderData->rightDB = decibelRSmoothed;
            pathFifo.finishedWrite();
        }
    }
    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }
    bool getPath(SpectrumRenderData& path)
    {
        if (auto* renderData = pathFifo.getReadBuffer())
        {
            path.spectrumPath = renderData->spectrumPath;
            path.peakHoldPath = renderData->peakHoldPath;
            path.leftDB = renderData->leftDB;
            path.rightDB = renderData->rightDB;

            pathFifo.finishedRead();
            return true;
        }
        return false;
    }
    std::vector<float> makeFreqLUT(const double sampleRate, const float minHz, const float maxHz) const
    {
        std::vector<float> frequencyLUT;
        frequencyLUT.reserve(FFT_SIZE_HALF);
        const float binWidth = static_cast<float>(sampleRate / FFT_SIZE);
        for (int i = 0; i < FFT_SIZE_HALF; ++i)
        {
            frequencyLUT.push_back(juce::mapFromLog10((binWidth * i), minHz, maxHz));
        }
        return frequencyLUT;
    }
private:
    static constexpr int FFT_ORDER = 12;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int FFT_SIZE_HALF = FFT_SIZE >> 1;
    SingleChannelSampleFifo* channelFifoL;
    SingleChannelSampleFifo* channelFifoR;
    juce::AudioBuffer<float> monoBufferL;
    juce::AudioBuffer<float> monoBufferR;
    juce::AudioBuffer<float> fftBuffer;
    std::vector<float> decibelsCurrent;
    std::vector<float> windowTable;
    zlth::FFT<FFT_ORDER> fft;
    std::vector<float> decibelsPeak;
    std::vector<float> gainsBuffer;
    float decibelLCurrent = 0.0f;
    float decibelRCurrent = 0.0f;
    float decibelLSmoothed = 0.0f;
    float decibelRSmoothed = 0.0f;
    Fifo<SpectrumRenderData> pathFifo;
};

class VisualizerComponent: public juce::Component, private juce::AsyncUpdater, public juce::AudioProcessorValueTreeState::Listener
{
public:
    VisualizerComponent(QuasarEQAudioProcessor& p):
        audioProcessor(p),
        pathProducer(audioProcessor.leftChannelFifo, audioProcessor.rightChannelFifo),
        analyzerThread(pathProducer, *this)
    {
        freqLUT = pathProducer.makeFreqLUT(audioProcessor.getSampleRate(), MIN_HZ, MAX_HZ);
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            const juce::String index = juce::String (i + 1);
            for (const auto& prefix : bandParamPrefixes)
            {
                audioProcessor.apvts.addParameterListener (prefix + index, this);
            }
        }
    };
    ~VisualizerComponent()
    {
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            const juce::String index = juce::String (i + 1);
            for (const auto& prefix : bandParamPrefixes)
            {
                audioProcessor.apvts.removeParameterListener (prefix + index, this);
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
            g.setColour(quasar::colours::audioSignal.withAlpha(0.45f));
            g.fillPath(curvePathPeak);
        }
        if (peakHoldPoints.size() != 0)
        {
            juce::Path curvePathPeak = createBezierPath(peakHoldPoints);
            g.setColour(quasar::colours::audioSignal);
            g.strokePath(curvePathPeak, juce::PathStrokeType(1.3f));
        }
        auto& apvts = audioProcessor.apvts;
        g.setColour(quasar::colours::enabled);
        g.strokePath(responseCurvePath, juce::PathStrokeType(2.5f));
        g.setFillType(juce::FillType(quasar::colours::audioSignal.withAlpha(0.3f)));
        g.fillPath(responseCurvePath);
        g.restoreState();
        const float high = 12.0f;
        const float low = -36.0f;
        const float lClamped = juce::jlimit(low, high, localPath.leftDB);
        const float rClamped = juce::jlimit(low, high, localPath.rightDB);
        const int leftY = juce::roundToInt(juce::jmap(lClamped, low, high, getLevelMeterArea().toFloat().getBottom(), getLevelMeterArea().toFloat().getY()));
        const int rightY = juce::roundToInt(juce::jmap(rClamped, low, high, getLevelMeterArea().toFloat().getBottom(), getLevelMeterArea().toFloat().getY()));
        g.setColour(quasar::colours::audioSignal.withAlpha(0.45f));
        g.fillRect(juce::Rectangle<int>::leftTopRightBottom(getLevelMeterArea().getX(), leftY, getLevelMeterArea().getX() + (getLevelMeterArea().getWidth() >> 1), getLevelMeterArea().getBottom()));
        g.fillRect(juce::Rectangle<int>::leftTopRightBottom(getLevelMeterArea().getX() + (getLevelMeterArea().getWidth() >> 1), rightY, getLevelMeterArea().getRight(), getLevelMeterArea().getBottom()));
        auto bounds = getCurveArea().toFloat();
        const float minDb = -24.0f;
        const float maxDb = 24.0f;
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            juce::String index = juce::String(i + 1);
            const auto bypass = apvts.getRawParameterValue(ID_PREFIX_BYPASS + index)->load();
            if (bypass < 0.5f)
            {
                float freqHz = apvts.getRawParameterValue(ID_PREFIX_FREQ + index)->load();
                float gainDb = apvts.getRawParameterValue(ID_PREFIX_GAIN + index)->load();
                float x = bounds.getX() + bounds.getWidth() * juce::mapFromLog10(freqHz, MIN_HZ, MAX_HZ);
                float y = juce::jmap(gainDb, minDb, maxDb, bounds.getBottom(), bounds.getY());
                g.setColour(quasar::colours::labelBackground);
                const int pointSize = 14;
                const int highLightPointSize = pointSize * 2;
                g.fillEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize);
                g.setColour(quasar::colours::staticText);
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
            for (int i = 0; i < NUM_BANDS; ++i)
            {
                juce::String index = juce::String(i + 1);
                bool isBypassed = apvts.getRawParameterValue(ID_PREFIX_BYPASS + index)->load() > 0.5f;
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
                    if (auto* bpParam = apvts.getParameter(ID_PREFIX_BYPASS + index))
                        bpParam->setValueNotifyingHost(0.0f);
                    if (auto* fParam = apvts.getParameter(ID_PREFIX_FREQ + index))
                        fParam->setValueNotifyingHost(apvts.getParameterRange(ID_PREFIX_FREQ + index).convertTo0to1(freqHz));
                    if (auto* gParam = apvts.getParameter(ID_PREFIX_GAIN + index))
                        gParam->setValueNotifyingHost(apvts.getParameterRange(ID_PREFIX_GAIN + index).convertTo0to1(gainDb));
                    if (auto* qParam = apvts.getParameter(ID_PREFIX_QUAL + index))
                        qParam->setValueNotifyingHost(qParam->getDefaultValue());
                    if (auto* tParam = apvts.getParameter(ID_PREFIX_TYPE + index))
                    {
                        float normalizedValue = 0.0f;
                        if (getSelectedTypeCallback)
                        {
                            normalizedValue = apvts.getParameterRange(ID_PREFIX_TYPE + index).convertTo0to1((float)getSelectedTypeCallback());
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
            if (auto* p1 = audioProcessor.apvts.getParameter(ID_PREFIX_FREQ + index)) p1->beginChangeGesture();
            if (auto* p2 = audioProcessor.apvts.getParameter(ID_PREFIX_GAIN + index)) p2->beginChangeGesture();
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
            if (auto* p1 = audioProcessor.apvts.getParameter(ID_PREFIX_FREQ + index)) p1->endChangeGesture();
            if (auto* p2 = audioProcessor.apvts.getParameter(ID_PREFIX_GAIN + index)) p2->endChangeGesture();
            draggingBand = -1;
        }
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const int bandIdx = getClosestBand(e.position);
        if (bandIdx == -1) return;
        const juce::String paramID = ID_PREFIX_QUAL + juce::String(bandIdx + 1);
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
        AnalyzerThread(PathProducer& producer, VisualizerComponent& comp): juce::Thread("FFT Analyzer Thread"), producer(producer), responseCurveComponent(comp)
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
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            juce::String index = juce::String(i + 1);
            if (apvts.getRawParameterValue(ID_PREFIX_BYPASS + index)->load() > 0.5f)
                continue;
            float freqHz = apvts.getRawParameterValue(ID_PREFIX_FREQ + index)->load();
            float gainDb = apvts.getRawParameterValue(ID_PREFIX_GAIN + index)->load();
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
        if (auto* freqParam = audioProcessor.apvts.getParameter(ID_PREFIX_FREQ + index))
        {
            freqParam->setValueNotifyingHost(audioProcessor.apvts.getParameterRange(ID_PREFIX_FREQ + index).convertTo0to1(freqHz));
        }
        if (auto* gainParam = audioProcessor.apvts.getParameter(ID_PREFIX_GAIN + index))
        {
            gainParam->setValueNotifyingHost(audioProcessor.apvts.getParameterRange(ID_PREFIX_GAIN + index).convertTo0to1(gainDb));
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
        auto formatFreqText = [](float f) { return (f < 1000.0f) ? juce::String (static_cast<int> (f)) : juce::String (static_cast<int> (f / 1000.0f)) + "k"; };
        auto formatDbText = [](float db) { return (db > 0 ? "+" : "") + juce::String (static_cast<int> (db)); };
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
        g.drawHorizontalLine(dbToY(0.0f, meterArea, METER_MAX, METER_MIN), meterAreaF.getX(), meterAreaF.getRight());
        g.drawVerticalLine(meterAreaF.getCentreX(), meterAreaF.getY(), meterAreaF.getBottom());
        for (const auto& m : xMarkers)g.drawVerticalLine(m.pos, curveAreaF.getY(), curveAreaF.getBottom());
        for (const auto& m : curveYMarkers)g.drawHorizontalLine(m.pos, curveAreaF.getX(), curveAreaF.getRight());
        g.drawRect(meterArea);
        g.setColour(quasar::colours::staticText);
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
        const int curveSize = getCurveArea().getWidth();
        double sr = audioProcessor.getSampleRate();
        responseCurveMagnitude.assign(curveSize, 0.0f);

        auto svfParamsList = audioProcessor.getSvfParams();

        for (int i = 0; i < curveSize; ++i)
        {
            float normalizedX = (float)i / (float)(curveSize - 1);
            float freqHz = juce::mapToLog10(normalizedX, (float)MIN_HZ, (float)MAX_HZ);
            float totalGainLinear = 1.0f;

            for (const auto& p : svfParamsList)
            {
                zlth::dsp::filter::ZdfSvfFilter tempFilter;
                tempFilter.update_coefficients(p.type, p.freq, p.q, p.gainDb, (float)sr);

                totalGainLinear *= tempFilter.get_magnitude(freqHz, (float)sr);
            }

            responseCurveMagnitude[i] = juce::Decibels::gainToDecibels(totalGainLinear);
        }
        auto bounds = getCurveArea().toFloat();
        const float minDb = EDITOR_MIN_DBFS;
        const float maxDb = EDITOR_MAX_DBFS;
        responseCurvePath.clear();
        responseCurvePath.startNewSubPath(getLocalBounds().toFloat().getX(), getLocalBounds().toFloat().getCentreY());
        for (size_t i = 0; i < responseCurveMagnitude.size(); ++i)
        {

            float magnitudeDb = responseCurveMagnitude[i];
            float normalizedX = (float)i / (float)(responseCurveMagnitude.size() - 1);
            float x = bounds.getX() + bounds.getWidth() * normalizedX;
            float y = juce::jmap(magnitudeDb, minDb, maxDb, bounds.getBottom(), bounds.getY());

            responseCurvePath.lineTo(x, y);
        }
        responseCurvePath.lineTo(getLocalBounds().toFloat().getRight(), getLocalBounds().toFloat().getCentreY());
    };
    int draggingBand = -1;
    bool parametersNeedUpdate = true;
    static constexpr float MIN_DBFS = -90.0f;
    static constexpr float MAX_DBFS = 30.0f;
    static constexpr int HALF_FONT_HEIGHT = 5;
    static constexpr int FONT_HEIGHT = HALF_FONT_HEIGHT * 2;
    static constexpr int margin = 10;
    static constexpr int THREAD_SLEEP_TIME = 20;
    QuasarEQAudioProcessor& audioProcessor;
    PathProducer pathProducer;
    SpectrumRenderData channelPathToDraw;
    std::vector<juce::Point<float>> spectrumPoints;
    std::vector<juce::Point<float>> peakHoldPoints;
    std::vector<float> freqLUT;
    juce::CriticalSection pathLock;
    juce::Path responseCurvePath;
    AnalyzerThread analyzerThread;
    std::vector<float> responseCurveMagnitude;
};




class QuasarEQAudioProcessorEditor: public juce::AudioProcessorEditor
{
public:
    QuasarEQAudioProcessorEditor(QuasarEQAudioProcessor& p): AudioProcessorEditor(&p), audioProcessor(p), visualizerComponent(p)
    {
        setLookAndFeel(&customLNF);
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            bandControls.push_back(std::make_unique<FilterBandControl>(audioProcessor.apvts, i));
            addAndMakeVisible(*bandControls.back());
        }

        struct IconData { const char* data; int size; };
        std::vector<IconData> icons = {
            {BinaryData::hp_svg, BinaryData::hp_svgSize},
            {BinaryData::hs_svg, BinaryData::hs_svgSize},
            {BinaryData::lp_svg, BinaryData::lp_svgSize},
            {BinaryData::ls_svg, BinaryData::ls_svgSize},
            {BinaryData::peak_svg, BinaryData::peak_svgSize},
            {BinaryData::tilt_svg, BinaryData::tilt_svgSize}
        };

        for (int i = 0; i < 6; ++i)
        {
            auto btn = std::make_unique<CustomIconButton>(icons[i].data, icons[i].size);
            btn->setRadioGroupId(1001);
            btn->setClickingTogglesState(true);
            btn->onClick = [this, i]
                {
                    if (paletteButtons[i]->getToggleState())
                        selectedFilterType = i;
                };

            addAndMakeVisible(*btn);
            paletteButtons.push_back(std::move(btn));
        }
        paletteButtons[selectedFilterType]->setToggleState(true, juce::dontSendNotification);

        visualizerComponent.getSelectedTypeCallback = [this] { return selectedFilterType; };

        pluginInfoLabel.setText("Quasar EQ 2", juce::dontSendNotification);
        pluginInfoLabel.setJustificationType(juce::Justification::centredLeft);
        pluginInfoLabel.setFont(16.0f);
        gainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
        gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
        addAndMakeVisible(visualizerComponent);
        addAndMakeVisible(pluginInfoLabel);
        addAndMakeVisible(gainSlider);
        outGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, ID_GAIN, gainSlider);
        setSize(657, windowHeight);
    };
    ~QuasarEQAudioProcessorEditor()
    {
        setLookAndFeel(nullptr);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(BACKGROUND_COLOR);
    }
    static constexpr int margin = 4;
    static constexpr int topSectionH = 38;
    static constexpr int uopSectionH = 42;
    static constexpr int midSectionH = 300;
    static constexpr int botSectionH = 300;
    static constexpr int windowHeight = margin * 2 + topSectionH + uopSectionH + midSectionH + botSectionH;
    void resized() override
    {
        juce::Rectangle<int> mainArea = getLocalBounds().reduced(margin);
        juce::Rectangle<int> top = mainArea.removeFromTop(topSectionH).reduced(margin);
        juce::Rectangle<int> uop = mainArea.removeFromTop(uopSectionH).reduced(margin);
        juce::Rectangle<int> mid = mainArea.removeFromTop(midSectionH).reduced(margin);
        juce::Rectangle<int> bot = mainArea.removeFromTop(botSectionH).reduced(margin);
        const int sideSize = 55;
        pluginInfoLabel.setBounds(top.reduced(margin));

        uop.reduce(margin, margin);
        uop.removeFromRight(uop.getWidth() / 1.75f);
        const int numButtons = static_cast<int>(paletteButtons.size());
        int btnW = uop.getWidth() / numButtons;
        for (auto& btn : paletteButtons)
        {
            if (btn)
                btn->setBounds(uop.removeFromLeft(btnW).reduced(1));
        }

        visualizerComponent.setBounds(mid);
        gainSlider.setBounds(bot.removeFromRight(20 * 3).reduced(margin));
        bot.reduce(margin, margin);
        const int bandWidth = bot.getWidth() / NUM_BANDS;
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            if (bandControls[i])
            {
                bandControls[i]->setBounds(bot.removeFromLeft(bandWidth));
            }
        }
    };
private:
    const juce::Colour BACKGROUND_COLOR = juce::Colour(juce::uint8(40), juce::uint8(42), juce::uint8(50));
    class CustomSlider: public juce::Slider { public:void mouseDoubleClick (const juce::MouseEvent& event) override {}; };
    class CustomButton: public juce::Button
    {
    public:
        CustomButton(): juce::Button("PowerButton") {}
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
        {
            const auto isBypass = getToggleState();
            const auto color = isBypass ? quasar::colours::enabled : quasar::colours::disabled;
            auto bounds = getLocalBounds().toFloat();
            g.setColour(juce::Colours::black);
            g.fillRect(bounds);
            g.setColour(color);
            g.drawRect(bounds, 2.0f);
            g.setFont(12.0f);
            g.drawText("Bypass", getLocalBounds(), juce::Justification::centred);
        }
        void mouseEnter(const juce::MouseEvent& event) override
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }
        void mouseExit(const juce::MouseEvent& event) override
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
    };
    class FilterBandControl: public juce::Component
    {
    public:
        FilterBandControl(juce::AudioProcessorValueTreeState& apvts, int bandIndex)
        {
            typeComboBox.setJustificationType(juce::Justification::centred);
            typeComboBox.addItemList (filterTags, 1);
            bypassButton.setClickingTogglesState(true);
            for (auto* s : {&freqSlider, &gainSlider, &qSlider})
            {
                s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 16);
            }
            for (auto* c : allComponents)
            {
                addAndMakeVisible(c);
            }
            const juce::String index = juce::String(bandIndex + 1);
            freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_PREFIX_FREQ + index, freqSlider);
            gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_PREFIX_GAIN + index, gainSlider);
            qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, ID_PREFIX_QUAL + index, qSlider);
            typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ID_PREFIX_TYPE + index, typeComboBox);
            bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ID_PREFIX_BYPASS + index, bypassButton);
        };
        ~FilterBandControl() override {};
        void resized() override
        {
            auto bounds = getLocalBounds();
            auto topHeader = bounds.removeFromTop(30);
            bypassButton.setBounds(topHeader.reduced(margin));
            auto secHeader = bounds.removeFromTop(30);
            typeComboBox.setBounds(secHeader.reduced(margin));
            bounds.reduce(margin, margin);
            int controlHeight = bounds.getHeight() / 3;
            freqSlider.setBounds(bounds.removeFromTop(controlHeight).reduced(margin));
            gainSlider.setBounds(bounds.removeFromTop(controlHeight).reduced(margin));
            qSlider.setBounds(bounds.reduced(margin));
        };
    private:
        std::vector<juce::Component*> allComponents {&typeComboBox, &bypassButton, &freqSlider, &gainSlider, &qSlider};
        CustomButton bypassButton;
        juce::Slider freqSlider;
        juce::Slider gainSlider;
        juce::Slider qSlider;
        juce::ComboBox typeComboBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    };
    class CustomIconButton: public juce::Button
    {
    public:
        CustomIconButton(const char* svgData, int svgSize): juce::Button("IconButton")
        {
            if (svgData != nullptr)
            {
                drawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse(juce::String::fromUTF8(svgData, svgSize)));
            }
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour(juce::Colours::black);
            g.fillRect(bounds);
            if (getToggleState())
            {
                g.setColour(quasar::colours::enabled);
                g.drawRect(bounds, 1.0f);
            }
            if (drawable != nullptr)
            {
                drawable->replaceColour(juce::Colours::black, juce::Colours::white);
                drawable->drawWithin(g, bounds.reduced(6.0f), juce::RectanglePlacement::centred, 1.0f);
            }
        }
    private:
        std::unique_ptr<juce::Drawable> drawable;
    };
    CustomLNF customLNF;
    CustomSlider gainSlider;
    QuasarEQAudioProcessor& audioProcessor;
    VisualizerComponent visualizerComponent;
    juce::Label pluginInfoLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outGainAttachment;
    std::vector<std::unique_ptr<FilterBandControl>> bandControls;

    std::vector<std::unique_ptr<CustomIconButton>> paletteButtons;
    int selectedFilterType = 4;
};
