#include "SpectrumAnalyzer.h"
#include "CustomLookAndFeel.h"
#include "../PluginProcessor.h"

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer (AudioPluginAudioProcessor& proc)
    : processor (proc), highpassFreq (*proc.getAPVTS().getRawParameterValue (ParameterSpecs::highpassFreq.id)),
      lowpassFreq (*proc.getAPVTS().getRawParameterValue (ParameterSpecs::lowpassFreq.id))
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

juce::Path SpectrumAnalyzer::filtersCurve() const
{
    const auto bounds = getLocalBounds().toFloat();

    juce::Path path;

    for (int band = 0; band < SpectrumFrame::numBands; ++band)
    {
        const auto db = Effect::filtersDb (SpectrumFrame::Analyser::centreOf (band), shown.highpass, shown.lowpass);
        const juce::Point<float> point { bounds.getWidth() * (static_cast<float> (band) + 0.5f) / SpectrumFrame::numBands,
                                         filtersInset + bounds.getHeight() * (1.0f - SpectrumFrame::proportion (SpectrumFrame::topDb + db)) };

        if (band == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }

    return path;
}

//==============================================================================
void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto red = juce::Colour (CustomLookAndFeel::primaryRed);
    const auto offWhite = juce::Colour (CustomLookAndFeel::offWhite);

    const auto filled = [&bounds] (juce::Path path)
    {
        path.lineTo (bounds.getBottomRight());
        path.lineTo (bounds.getBottomLeft());
        path.closeSubPath();
        return path;
    };

    // The dry signal is a shape with no outline, to tell it from the filters' curve
    g.setColour (offWhite.withAlpha (0.12f));
    g.fillPath (filled (curve (SpectrumFrame::dryOffset)));

    const auto wet = curve (SpectrumFrame::wetOffset);

    g.setGradientFill (juce::ColourGradient::vertical (red.withAlpha (0.4f), 0.0f, red.withAlpha (0.1f), bounds.getHeight()));
    g.fillPath (filled (wet));

    g.setColour (red);
    g.strokePath (wet, juce::PathStrokeType (2.0f));

    g.setColour (offWhite.withAlpha (0.25f));
    g.strokePath (filtersCurve(), juce::PathStrokeType (1.0f));

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
    const Cutoffs cutoffs { highpassFreq.load(), lowpassFreq.load() };
    const auto cutoffMoved = cutoffs != std::exchange (shown, cutoffs);

    if (frameChanged || cutoffMoved)
        repaint();
}
