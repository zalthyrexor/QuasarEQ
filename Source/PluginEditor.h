#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
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
    PathProducer(SingleChannelSampleFifo& leftScsf, SingleChannelSampleFifo& rightScsf): leftChannelFifo(&leftScsf), rightChannelFifo(&rightScsf)
    {
        fftBuffer.setSize(1, FFT_OUT_SIZE, false, true, true);
        monoBufferL.setSize(1, FFT_SIZE, false, true, true);
        monoBufferR.setSize(1, FFT_SIZE, false, true, true);
        monoAverageBuffer.setSize(1, FFT_SIZE, false, true, true);
        peakFallVelocity.assign(RENDER_OUT_SIZE, 0.0f);
        peakHoldDecibels.assign(RENDER_OUT_SIZE, -std::numeric_limits<float>::infinity());
        currentDecibels.assign(RENDER_OUT_SIZE, -std::numeric_limits<float>::infinity());
        Gains.assign(RENDER_OUT_SIZE, 0.0f);
        SmoothGains.assign(RENDER_OUT_SIZE, 0.0f);
    };
    void process(double sampleRate)
    {
        juce::AudioBuffer<float> leftIncomingBuffer, rightIncomingBuffer;
        bool aaa = false;
        while (leftChannelFifo->getNumCompleteBuffersAvailable() > 0 && rightChannelFifo->getNumCompleteBuffersAvailable() > 0)
        {
            if (leftChannelFifo->getAudioBuffer(leftIncomingBuffer) && rightChannelFifo->getAudioBuffer(rightIncomingBuffer))
            {
                aaa = true;
                const int incomingSize = leftIncomingBuffer.getNumSamples();
                currentLeftGain = leftIncomingBuffer.getMagnitude(0, 0, incomingSize);
                currentRightGain = rightIncomingBuffer.getMagnitude(0, 0, incomingSize);
                const int copySize = FFT_SIZE - incomingSize;
                monoBufferL.copyFrom(0, 0, monoBufferL.getReadPointer(0, incomingSize), copySize);
                monoBufferR.copyFrom(0, 0, monoBufferR.getReadPointer(0, incomingSize), copySize);
                monoBufferL.copyFrom(0, copySize, leftIncomingBuffer.getReadPointer(0), incomingSize);
                monoBufferR.copyFrom(0, copySize, rightIncomingBuffer.getReadPointer(0), incomingSize);
                auto* destData = monoAverageBuffer.getWritePointer(0);
                juce::FloatVectorOperations::copy(destData, monoBufferL.getReadPointer(0), FFT_SIZE);
                juce::FloatVectorOperations::add(destData, monoBufferR.getReadPointer(0), FFT_SIZE);
                juce::FloatVectorOperations::multiply(destData, 0.5f, FFT_SIZE);
                auto* fftDataWritePointer = fftBuffer.getWritePointer(0);
                juce::FloatVectorOperations::clear(fftDataWritePointer, FFT_OUT_SIZE);
                juce::FloatVectorOperations::copy(fftDataWritePointer, monoAverageBuffer.getReadPointer(0), FFT_SIZE);
                windowing.multiplyWithWindowingTable(fftDataWritePointer, FFT_SIZE);
                fft.performFrequencyOnlyForwardTransform(fftDataWritePointer);
                juce::FloatVectorOperations::multiply(fftDataWritePointer, fftDataWritePointer, INVERSE_NUM_BINS, NUM_BINS);
                generatePath(fftDataWritePointer, static_cast<float>(incomingSize) / sampleRate);
            }
        }
        if (aaa)
        {
            pathFifo.push({currentDecibels, peakHoldDecibels, juce::Decibels::gainToDecibels(smoothedLeftGain), juce::Decibels::gainToDecibels(smoothedRightGain)});
        }
    };
    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }
    ;
    bool getPath(SpectrumRenderData& path)
    {
        return pathFifo.pull(path);
    };
    std::vector<float> makeFreqLUT(const double sampleRate, const float minHz, const float maxHz) const
    {
        std::vector<float> frequencyLUT;
        frequencyLUT.reserve(RENDER_OUT_SIZE);
        const float binWidth = static_cast<float>(sampleRate / FFT_SIZE);
        for (int levelIndex = 0, sourceDataIndex = 0, outputIndex = 0; levelIndex < NUM_SECTIONS; ++levelIndex)
        {
            const int windowSize = 1 << levelIndex;
            const int nextOutputStart = outputIndex + (SECTION_SIZE >> levelIndex);
            for (; outputIndex < nextOutputStart; ++outputIndex)
            {

                frequencyLUT.push_back(juce::mapFromLog10((binWidth * sourceDataIndex), minHz, maxHz));
                sourceDataIndex += windowSize;
            }
        }
        return frequencyLUT;
    };
private:
    static constexpr int FFT_ORDER = 12;
    static constexpr int NUM_SECTIONS = 1 << 3;
    static constexpr int SECTION_SIZE = 1 << 8;
    static constexpr int NUM_BINS = 1 << (FFT_ORDER - 1);
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int FFT_OUT_SIZE = 1 << (FFT_ORDER + 1);
    static constexpr int RENDER_OUT_SIZE = 510;
    static constexpr float INVERSE_NUM_BINS = 1.0f / (1 << (FFT_ORDER - 1));
    static constexpr float SMOOTHING_TIME_CONSTANT = 0.02f;
    static constexpr float PEAK_DECAY_RATE = 80.0f;
    static constexpr float LEVEL_METER_SMOOTHING_TIME_CONSTANT = SMOOTHING_TIME_CONSTANT * 5.0f;
    SingleChannelSampleFifo* leftChannelFifo;
    SingleChannelSampleFifo* rightChannelFifo;
    juce::AudioBuffer<float> monoBufferL;
    juce::AudioBuffer<float> monoBufferR;
    juce::AudioBuffer<float> fftBuffer;
    juce::AudioBuffer<float> monoAverageBuffer;
    juce::dsp::FFT fft {FFT_ORDER};
    juce::dsp::WindowingFunction<float> windowing {size_t(FFT_SIZE), juce::dsp::WindowingFunction<float>::blackmanHarris, true};
    std::vector<float> peakFallVelocity;
    std::vector<float> peakHoldDecibels;
    std::vector<float> Gains;
    std::vector<float> SmoothGains;
    std::vector<float> currentDecibels;
    float currentLeftGain = 0.0f;
    float currentRightGain = 0.0f;
    float smoothedLeftGain = 0.0f;
    float smoothedRightGain = 0.0f;
    Fifo<SpectrumRenderData> pathFifo;
    void generatePath(const float* renderData, const float deltaTime)
    {
        for (int levelIndex = 0, sourceDataIndex = 0, outputIndex = 0; levelIndex < NUM_SECTIONS; ++levelIndex)
        {
            const int windowSize = 1 << levelIndex;
            const int nextOutputStart = outputIndex + (SECTION_SIZE >> levelIndex);
            for (; outputIndex < nextOutputStart; ++outputIndex)
            {
                Gains[outputIndex] = *std::max_element(renderData + sourceDataIndex, renderData + sourceDataIndex + windowSize);
                sourceDataIndex += windowSize;
            }
        }
        const float peakFallRate = PEAK_DECAY_RATE * deltaTime;
        const float alphaSmooth = 1.0f - std::exp(-deltaTime / SMOOTHING_TIME_CONSTANT);
        const float oneMinusAlpha = 1.0f - alphaSmooth;
        for (size_t i = 0; i < RENDER_OUT_SIZE; ++i)
        {
            if (SmoothGains[i] > Gains[i])
            {
                SmoothGains[i] = alphaSmooth * Gains[i] + oneMinusAlpha * SmoothGains[i];
            }
            else
            {
                SmoothGains[i] = Gains[i];
            }
            currentDecibels[i] = juce::Decibels::gainToDecibels(SmoothGains[i]);
            peakFallVelocity[i] += peakFallRate;
            peakHoldDecibels[i] -= peakFallVelocity[i] * deltaTime;
            if (currentDecibels[i] >= peakHoldDecibels[i])
            {
                peakFallVelocity[i] = 0.0f;
                peakHoldDecibels[i] = currentDecibels[i];
            }
        }
        const float levelMeterAlphaSmooth = 1.0f - std::exp(-deltaTime / (LEVEL_METER_SMOOTHING_TIME_CONSTANT));
        const float levelMeterOneMinusAlpha = 1.0f - levelMeterAlphaSmooth;
        if (currentLeftGain < smoothedLeftGain)
        {
            smoothedLeftGain = levelMeterAlphaSmooth * currentLeftGain + levelMeterOneMinusAlpha * smoothedLeftGain;
        }
        else
        {
            smoothedLeftGain = currentLeftGain;
        }
        if (currentRightGain < smoothedRightGain)
        {
            smoothedRightGain = levelMeterAlphaSmooth * currentRightGain + levelMeterOneMinusAlpha * smoothedRightGain;
        }
        else
        {
            smoothedRightGain = currentRightGain;
        }
    };
};
namespace quasar
{
    namespace colours
    {
        const juce::Colour enabled {0xff7391ff};
        const juce::Colour groove {0xff000000};
        const juce::Colour disabled {0xff555555};
        const juce::Colour staticText {0xffd3d3d3};
        const juce::Colour labelBackground {0xff17171a};
        const juce::Colour audioSignal {0xff4d76ff};
    }
}

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
        const float high = 6.0f;
        const float low = -18.0f;
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
    static constexpr float METER_MAX = 6.0f;
    static constexpr float METER_MIN = -18.0f;
    static constexpr float MIN_HZ = 20.0f;
    static constexpr float MAX_HZ = 20000.0f;
    static constexpr float EDITOR_MIN_DBFS = -24.0f;
    static constexpr float EDITOR_MAX_DBFS = 24.0f;
    const std::vector<float> editorDBs = {24.0f, 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f};
    const std::vector<float> meterDBs = {6.0f, 3.0f, 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -15.0f, -18.0f};
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
    juce::Path createBezierPath(const std::vector<juce::Point<float>>& points)
    {
        const size_t numPoints = points.size();
        const size_t a = 255;

        const size_t b = 0;
        if ((numPoints < 2))
        {
            return juce::Path {};
        }
        static constexpr float BEZIER_SCALE = 1.0f / 6.0f;
        juce::Path p {};
        p.startNewSubPath(points[b]);
        p.lineTo(points[b + 1]);
        for (size_t i = b + 1; i < a - 2; ++i)
        {
            const auto& p0 = points[i - 1];
            const auto& p1 = points[i];
            const auto& p2 = points[i + 1];
            const auto& p3 = points[i + 2];
            const juce::Point<float> cp1 = p1 + (p2 - p0) * BEZIER_SCALE;
            const juce::Point<float> cp2 = p2 - (p3 - p1) * BEZIER_SCALE;
            p.cubicTo(cp1, cp2, p2);
        }
        p.lineTo(points[a - 1]);
        for (size_t i = a; i < 509; ++i)
        {
            p.lineTo(points[i]);
        }
        return p;
    };
    void calculateResponseCurve()
    {
        const int curveSize = getCurveArea().getWidth();
        const float minHz = MIN_HZ;
        const float maxHz = MAX_HZ;
        double sr = audioProcessor.getSampleRate();
        responseCurveMagnitude.clear();
        responseCurveMagnitude.resize(curveSize, 0.0f);
        auto coefsBuffer = audioProcessor.getCurrentCoefficients();
        for (int i = 0; i < curveSize; ++i)
        {
            float normalizedX = (float)i / (float)(curveSize - 1);
            float freqHz = juce::mapToLog10(normalizedX, minHz, maxHz);
            float totalGainLinear = 1.0f;
            for (const auto& coefs : coefsBuffer)
            {
                totalGainLinear *= coefs->getMagnitudeForFrequency(freqHz, sr);
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


class CustomLNF: public juce::LookAndFeel_V4
{
public:
    CustomLNF()
    {
        setColour(juce::Label::textColourId, quasar::colours::staticText);
        setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::black);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        auto center = juce::Rectangle<float>(x, y, w, h).getCentre();
        auto centerX = center.x;
        auto centerY = center.y;
        juce::Path backgroundArc;
        juce::Path valueArc;
        juce::Path pointer;
        auto size = juce::jmin(w, h);
        auto sliderBounds = juce::Rectangle<float>(size, size);
        sliderBounds.setCentre(center);
        sliderBounds = sliderBounds.reduced(5.0f);
        auto radius = sliderBounds.getWidth() / 2.0f;
        auto lineThickness = 3.5f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centerAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * 0.5f;
        g.setColour(quasar::colours::labelBackground);
        auto knobRadius = radius - lineThickness - 2.0f;
        g.fillEllipse(sliderBounds.reduced(lineThickness + 2.0f));
        backgroundArc.addCentredArc(centerX, centerY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(quasar::colours::groove.withAlpha(0.3f));
        g.strokePath(backgroundArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        valueArc.addCentredArc(centerX, centerY, radius, radius, 0.0f, centerAngle, toAngle, true);
        g.setColour(quasar::colours::enabled);
        g.strokePath(valueArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white);
        auto pointerWidth = 2.0f;
        auto pointerLength = 6.0f;
        pointer.addRoundedRectangle(-pointerWidth * 0.5f, -knobRadius, pointerWidth, pointerLength, 1.0f);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centerX, centerY));
        g.fillPath(pointer);
    }
    void drawComboBox(juce::Graphics& g, int w, int h, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        const auto color = quasar::colours::labelBackground;
        const auto bounds = juce::Rectangle<int>(0, 0, w, h).toFloat();
        g.setColour(color);
        g.fillRect(bounds);
    }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(1, 1, box.getWidth() - 2, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(juce::Justification::centred);
    }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
        float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(10.0f, 5.0f);
        float trackWidth = 6.0f;
        auto track = bounds.withSizeKeepingCentre(trackWidth, bounds.getHeight());
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(track, trackWidth * 0.5f);
        float zeroPos = (minSliderPos + maxSliderPos) * 0.5f;
        auto top = juce::jmin(zeroPos, sliderPos);
        auto bottom = juce::jmax(zeroPos, sliderPos);
        auto valueRect = track.withTop(top).withBottom(bottom);
        g.setColour(quasar::colours::enabled);
        g.fillRoundedRectangle(valueRect, trackWidth * 0.5f);
        auto thumbHeight = 12.0f;
        auto thumbWidth = 20.0f;
        auto thumbRect = juce::Rectangle<float>(thumbWidth, thumbHeight);
        thumbRect.setCentre(track.getCentreX(), sliderPos);
        g.setColour(quasar::colours::labelBackground);
        g.fillRoundedRectangle(thumbRect, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawRoundedRectangle(thumbRect, 2.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.fillRect(thumbRect.withSizeKeepingCentre(thumbWidth * 0.6f, 1.5f));
    }
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
            {BinaryData::peak_svg, BinaryData::peak_svgSize}
        };

        for (int i = 0; i < 5; ++i)
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

        pluginInfoLabel.setText("Quasar EQ", juce::dontSendNotification);
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
    QuasarEQAudioProcessorEditor::~QuasarEQAudioProcessorEditor()
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
        uop.removeFromRight(uop.getWidth() / 1.5f);
        int btnW = uop.getWidth() / 5;

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
