#pragma once

#include <JuceHeader.h>
#include "config.h"

class CustomLNF: public juce::LookAndFeel_V4 {
public:
  CustomLNF() {
    setColour(juce::Label::textColourId, config::text);
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
    g.setColour(config::slider);
    auto knobRadius = radius - lineThickness - 2.0f;

    auto knobBounds = sliderBounds.reduced(lineThickness + 2.0f);

    g.fillEllipse(knobBounds);
    g.setColour(config::sliderRim);
    g.drawEllipse(knobBounds, 2.0f);

    backgroundArc.addCentredArc(centerX, centerY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(config::groove);
    g.strokePath(backgroundArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    valueArc.addCentredArc(centerX, centerY, radius, radius, 0.0f, centerAngle, toAngle, true);
    g.setColour(config::theme);
    g.strokePath(valueArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    g.setColour(config::sliderPointer);
    auto pointerWidth = 2.0f;
    auto pointerLength = 6.0f;
    pointer.addRoundedRectangle(-pointerWidth * 0.5f, -knobRadius, pointerWidth, pointerLength, 1.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centerX, centerY));
    g.fillPath(pointer);
  }
  void drawComboBox(juce::Graphics& g, int w, int h, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override {
    const auto color = config::textBackground;
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
    auto bounds {juce::Rectangle<float>(x, y, w, h).reduced(10.0f, 5.0f)};
    auto track = bounds.withSizeKeepingCentre(6.0f, h);
    g.setColour(config::groove);
    g.fillRect(track);
    float zeroPos = (min + max) * 0.5f;
    auto top = juce::jmin(zeroPos, pos);
    auto bottom = juce::jmax(zeroPos, pos);
    auto valueRect = track.withTop(top).withBottom(bottom);
    g.setColour(config::theme);
    g.fillRect(valueRect);
    auto thumbRect {juce::Rectangle<float>(w, 12.0f)};
    thumbRect.setCentre(track.getCentreX(), pos);
    g.setColour(config::slider);
    g.fillRect(thumbRect);
    g.setColour(config::sliderRim);
    g.drawRect(thumbRect, 2.0f);
    g.setColour(config::sliderPointer);
    g.fillRect(thumbRect.withSizeKeepingCentre(w / 4.0f, 2.0f));
  }
};
