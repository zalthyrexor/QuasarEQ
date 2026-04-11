#pragma once

#include <JuceHeader.h>
#include "zlth_ui_colors.h"

class CustomLNF: public juce::LookAndFeel_V4
{
public:
    CustomLNF() {
        setColour(juce::Label::textColourId, juce::Colour(zlth::ui::colors::text));
        setColour(juce::Label::backgroundWhenEditingColourId, juce::Colours::black);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override {
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
        g.setColour(juce::Colour(zlth::ui::colors::slider));
        auto knobRadius = radius - lineThickness - 2.0f;

        auto knobBounds = sliderBounds.reduced(lineThickness + 2.0f);

        g.fillEllipse(knobBounds);
        g.setColour(juce::Colour(zlth::ui::colors::sliderRim));
        g.drawEllipse(knobBounds, 2.0f);

        backgroundArc.addCentredArc(centerX, centerY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(zlth::ui::colors::groove));
        g.strokePath(backgroundArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        valueArc.addCentredArc(centerX, centerY, radius, radius, 0.0f, centerAngle, toAngle, true);
        g.setColour(juce::Colour(zlth::ui::colors::theme));
        g.strokePath(valueArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour(juce::Colour(zlth::ui::colors::sliderPointer));
        auto pointerWidth = 2.0f;
        auto pointerLength = 6.0f;
        pointer.addRoundedRectangle(-pointerWidth * 0.5f, -knobRadius, pointerWidth, pointerLength, 1.0f);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centerX, centerY));
        g.fillPath(pointer);
    }
    void drawComboBox(juce::Graphics& g, int w, int h, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override {
        const auto color = juce::Colour(zlth::ui::colors::textBackground);
        const auto bounds = juce::Rectangle<int>(0, 0, w, h).toFloat();
        g.setColour(color);
        g.fillRect(bounds);
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRect(bounds);
    }
    juce::Font getComboBoxFont(juce::ComboBox& box) override {
        return {13.0f};
    }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(1, 1, box.getWidth() - 2, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(juce::Justification::centred);
    }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max, const juce::Slider::SliderStyle style, juce::Slider& slider) override {
        auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(10.0f, 5.0f);
        float trackWidth = 6.0f;
        auto track = bounds.withSizeKeepingCentre(trackWidth, bounds.getHeight());
        g.setColour(juce::Colour(zlth::ui::colors::groove));
        g.fillRect(track);
        float zeroPos = (min + max) * 0.5f;
        auto top = juce::jmin(zeroPos, pos);
        auto bottom = juce::jmax(zeroPos, pos);
        auto valueRect = track.withTop(top).withBottom(bottom);
        g.setColour(juce::Colour(zlth::ui::colors::theme));
        g.fillRect(valueRect);
        auto thumbHeight = 12.0f;
        auto thumbWidth = 20.0f;
        auto thumbRect = juce::Rectangle<float>(thumbWidth, thumbHeight);
        thumbRect.setCentre(track.getCentreX(), pos);
        g.setColour(juce::Colour(zlth::ui::colors::slider));
        g.fillRect(thumbRect);
        g.setColour(juce::Colour(zlth::ui::colors::sliderRim));
        g.drawRect(thumbRect, 2.0f);
        g.setColour(juce::Colour(zlth::ui::colors::sliderPointer));
        g.fillRect(thumbRect.withSizeKeepingCentre(8.0f, 2.0f));
    }
};
