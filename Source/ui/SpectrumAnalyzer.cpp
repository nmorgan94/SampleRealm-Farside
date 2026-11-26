#include "SpectrumAnalyzer.h"
#include "CustomLookAndFeel.h"
#include "../PluginProcessor.h"

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer (AudioPluginAudioProcessor& proc)
    : processor (proc), highpassFreq (*proc.getAPVTS().getRawParameterValue (ParameterSpecs::highpassFreq.id))
{
    setOpaque (false);
    startTimerHz (SpectrumFrame::updateHz);
}

juce::Path SpectrumAnalyzer::curve (int offset) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto& frame = source.getFrame();

    const auto pointAt = [&] (int band)
    {
        const auto level = frame[static_cast<size_t> (offset + band)];
        return juce::Point<float> (bounds.getWidth() * (static_cast<float> (band) + 0.5f) / SpectrumFrame::numBands,
                                   bounds.getHeight() * (1.0f - SpectrumFrame::proportion (level)));
    };

    // Through the middle of each pair of points, bending at the points themselves
    juce::Path path;
    auto previous = pointAt (0);
    path.startNewSubPath (0.0f, previous.y);
    path.lineTo (previous);

    for (int band = 1; band < SpectrumFrame::numBands; ++band)
    {
        const auto next = pointAt (band);
        path.quadraticTo (previous, (previous + next) * 0.5f);
        previous = next;
    }

    path.lineTo (previous);
    path.lineTo (bounds.getWidth(), previous.y);
    return path;
}

//==============================================================================
void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto red = juce::Colour (CustomLookAndFeel::primaryRed);
    const auto offWhite = juce::Colour (CustomLookAndFeel::offWhite);

    g.setColour (offWhite.withAlpha (0.35f));
    g.strokePath (curve (SpectrumFrame::dryOffset), juce::PathStrokeType (1.0f));

    const auto wet = curve (SpectrumFrame::wetOffset);

    auto fill = wet;
    fill.lineTo (bounds.getBottomRight());
    fill.lineTo (bounds.getBottomLeft());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient::vertical (red.withAlpha (0.4f), 0.0f, red.withAlpha (0.1f), bounds.getHeight()));
    g.fillPath (fill);

    g.setColour (red);
    g.strokePath (wet, juce::PathStrokeType (2.0f));

    // At the bottom of its range the highpass is off
    if (shownHighpassFreq > ParameterSpecs::highpassFreq.min)
    {
        g.setColour (offWhite.withAlpha (0.5f));
        g.fillRect (std::floor (bounds.getWidth() * SpectrumFrame::across (shownHighpassFreq)), 0.0f, 1.0f, bounds.getHeight());
    }

    // Down from the top, by as much as the duck is holding the wet signal down
    const auto ducked = 1.0f - source.getFrame()[SpectrumFrame::duckGain];

    if (ducked > 0.0f)
    {
        g.setColour (juce::Colour (CustomLookAndFeel::lightPink));
        g.fillRect (bounds.withLeft (bounds.getRight() - duckBarWidth).withHeight (bounds.getHeight() * ducked));
    }
}

//==============================================================================
void SpectrumAnalyzer::timerCallback()
{
    const auto frameChanged = source.update (processor.getSpectrumTap(), clock.seconds(), processor.getSampleRate(), processor.isSpectrumBypassed());
    const auto cutoff = highpassFreq.load();
    const auto cutoffMoved = ! juce::exactlyEqual (cutoff, std::exchange (shownHighpassFreq, cutoff));

    if (frameChanged || cutoffMoved)
        repaint();
}
