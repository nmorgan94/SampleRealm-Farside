#pragma once

#include <editor/editor.h>
#include "BinaryData.h"

class CustomLookAndFeel : public juce::LookAndFeel_V4,
                          public EditorFonts
{
public:
    static constexpr juce::uint32 primaryRed = 0xffe94560;
    static constexpr juce::uint32 lightPink = 0xffff6b9d;
    static constexpr juce::uint32 darkBlue = 0xff0f3460;
    static constexpr juce::uint32 offWhite = 0xfff0f0f0;
    static constexpr juce::uint32 backgroundTop = 0xff1a1a2e;
    static constexpr juce::uint32 backgroundBottom = 0xff16213e;

    CustomLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colour(primaryRed));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(primaryRed));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(darkBlue).withAlpha(0.6f));

        // The header's buttons and the overlays'
        setColour(juce::TextButton::buttonColourId, juce::Colour(darkBlue));
        setColour(juce::TextButton::textColourOffId, juce::Colour(offWhite));
        setColour(juce::TextButton::textColourOnId, juce::Colour(offWhite));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(primaryRed).withAlpha(0.6f));

        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(backgroundBottom));
        setColour(juce::PopupMenu::textColourId, juce::Colour(offWhite));
        setColour(juce::PopupMenu::headerTextColourId, juce::Colour(primaryRed));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(primaryRed).withAlpha(0.3f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(offWhite));

        // The overlays' text, and the box a knob's value is typed into
        setColour(juce::Label::textColourId, juce::Colour(offWhite));
        setColour(juce::Label::textWhenEditingColourId, juce::Colour(offWhite));
        setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(darkBlue));
        setColour(juce::Label::outlineWhenEditingColourId, juce::Colour(primaryRed));

        setColour(juce::TextEditor::backgroundColourId, juce::Colour(backgroundTop));
        setColour(juce::TextEditor::textColourId, juce::Colour(offWhite));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(primaryRed).withAlpha(0.6f));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(primaryRed));
        setColour(juce::TextEditor::highlightColourId, juce::Colour(primaryRed).withAlpha(0.4f));
        setColour(juce::CaretComponent::caretColourId, juce::Colour(primaryRed));
        setColour(juce::ScrollBar::thumbColourId, juce::Colour(primaryRed));

        setColour(ModalOverlay::dimColourId, juce::Colour(backgroundTop).withAlpha(0.75f));
        setColour(ModalOverlay::panelColourId, juce::Colour(backgroundBottom));
        setColour(ModalOverlay::outlineColourId, juce::Colour(primaryRed).withAlpha(0.6f));
        setColour(LevelMeter::backgroundColourId, juce::Colour(backgroundTop));
        setColour(LevelMeter::outlineColourId, juce::Colour(offWhite).withAlpha(0.4f));
        setColour(LevelMeter::barColourId, juce::Colour(offWhite));
        setColour(LevelMeter::clipColourId, juce::Colour(primaryRed));

        // The editor's gradient shows through
        setColour(HeaderBar::backgroundColourId, juce::Colours::black.withAlpha(0.35f));
        setColour(HeaderBar::titleColourId, juce::Colour(offWhite));
    }

    // A knob's name has a rounded box behind it
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        if (dynamic_cast<LabeledSlider*>(label.getParentComponent()) == nullptr)
        {
            LookAndFeel_V4::drawLabel(g, label);
            return;
        }

        g.setColour(juce::Colour(darkBlue).withAlpha(0.5f));
        g.fillRoundedRectangle(label.getLocalBounds().toFloat(), 5.0f);

        if (label.isBeingEdited())
            return;

        g.setColour(juce::Colour(primaryRed));
        g.setFont(getLabelFont(label));
        g.drawFittedText(label.getText(), label.getLocalBounds(), label.getJustificationType(), 1, label.getMinimumHorizontalScale());
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        const auto font = label.getFont();
        return getEditorFont(font.getHeight(), font.isBold());
    }

    // The header's title, the manual and the prompt's text field
    juce::Font getEditorFont(float height, bool bold) override
    {
        return juce::Font((bold ? orbitronBold() : orbitronRegular()).withHeight(height));
    }

    juce::Font getPopupMenuFont() override
    {
        return getEditorFont(13.0f, false);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override
    {
        // Scaled from a 70 px knob, so the header's small one is drawn the same
        auto scale = static_cast<float>(juce::jmin(width, height)) / 70.0f;
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10.0f * scale);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto lineW = juce::jmin(5.0f, radius * 0.2f);
        auto arcRadius = radius - lineW * 0.5f;

        // Draw outer glow effect
        g.setColour(juce::Colour(darkBlue).withAlpha(0.3f));
        g.fillEllipse(bounds.reduced(-5.0f * scale));

        // Draw full range outline (min to max path)
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(bounds.getCentreX(),
                                   bounds.getCentreY(),
                                   arcRadius,
                                   arcRadius,
                                   0.0f,
                                   rotaryStartAngle,
                                   rotaryEndAngle,
                                   true);

        g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw filled arc (current value) with gradient
        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc(bounds.getCentreX(),
                                  bounds.getCentreY(),
                                  arcRadius,
                                  arcRadius,
                                  0.0f,
                                  rotaryStartAngle,
                                  toAngle,
                                  true);

            // Gradient for the value arc
            juce::ColourGradient gradient(
                juce::Colour(primaryRed),
                bounds.getCentreX(), bounds.getY(),
                juce::Colour(lightPink),
                bounds.getCentreX(), bounds.getBottom(),
                false);
            g.setGradientFill(gradient);
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draw pointer/thumb
        juce::Path pointer;
        auto pointerLength = radius * 0.55f;
        auto pointerThickness = juce::jmin(3.0f, radius * 0.12f);
        auto cornerRadius = pointerThickness * 0.5f;
        auto pointerOffset = radius * 0.2f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -radius + lineW + pointerOffset, pointerThickness, pointerLength, cornerRadius);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(bounds.getCentreX(), bounds.getCentreY()));

        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillPath(pointer);

        // Draw center circle with gradient
        auto centerCircleSize = radius * 0.18f;
        juce::ColourGradient centerGradient(
            juce::Colour(primaryRed),
            bounds.getCentreX(), bounds.getCentreY() - centerCircleSize,
            juce::Colour(lightPink),
            bounds.getCentreX(), bounds.getCentreY() + centerCircleSize,
            false);
        g.setGradientFill(centerGradient);
        g.fillEllipse(bounds.getCentreX() - centerCircleSize,
                     bounds.getCentreY() - centerCircleSize,
                     centerCircleSize * 2.0f,
                     centerCircleSize * 2.0f);
    }

    // The text-only toggles are small; the header's buttons are the shared web editor's 12
    juce::Font getTextButtonFont(juce::TextButton& button, int) override
    {
        if (button.findColour(juce::TextButton::buttonColourId).isTransparent())
            return juce::Font(orbitronRegular().withPointHeight(7.0f));

        return getEditorFont(12.0f, false);
    }

    [[nodiscard]] static juce::FontOptions orbitronRegular()
    {
        static auto typeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::OrbitronRegular_ttf,
            BinaryData::OrbitronRegular_ttfSize);
        return juce::FontOptions(typeface);
    }
    
    [[nodiscard]] static juce::FontOptions orbitronBold()
    {
        static auto typeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::OrbitronBold_ttf,
            BinaryData::OrbitronBold_ttfSize);
        return juce::FontOptions(typeface);
    }
};
