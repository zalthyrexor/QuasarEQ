#include <JuceHeader.h>

#pragma once


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


class CustomLNF: public juce::LookAndFeel_V4
{
public:
    CustomLNF()
    {
        setColour(juce::Label::textColourId, quasar::colours::staticText);
        setColour(juce::Label::backgroundWhenEditingColourId, juce::Colours::black);
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