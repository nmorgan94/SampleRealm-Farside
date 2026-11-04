#include "SpectrumAnalyzer.h"
#include "../PluginProcessor.h"

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer(AudioPluginAudioProcessor& proc)
    : processor(proc)
{
    // Make component transparent
    setOpaque(false);
    
    startTimerHz(SpectrumScope::updateHz);
}

//==============================================================================
void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto width = bounds.getWidth();
    auto height = bounds.getHeight();
    
    // Draw frequency markers
    drawFrequencyMarkers(g, width, height, false);
    
    // Create spectrum path
    juce::Path spectrumPath;
    spectrumPath.startNewSubPath(0, height);
    
    for (int i = 0; i < SpectrumScope::scopeSize; ++i)
    {
        auto x = juce::jmap(static_cast<float>(i), 0.0f, static_cast<float>(SpectrumScope::scopeSize - 1), 0.0f, width);
        auto y = juce::jmap(scope.getScope()[static_cast<size_t>(i)], 0.0f, 1.0f, height, 0.0f);
        spectrumPath.lineTo(x, y);
    }
    
    // Draw the spectrum line
    g.setColour(juce::Colour(0xffe94560));
    g.strokePath(spectrumPath, juce::PathStrokeType(2.0f));
    
    // Draw filled area with gradient
    spectrumPath.lineTo(width, height);
    spectrumPath.closeSubPath();
    
    juce::ColourGradient gradient(
        juce::Colour(0xffe94560).withAlpha(0.4f), width / 2, 0,
        juce::Colour(0xffe94560).withAlpha(0.1f), width / 2, height,
        false);
    g.setGradientFill(gradient);
    g.fillPath(spectrumPath);
}

void SpectrumAnalyzer::drawFrequencyMarkers(juce::Graphics& g, float width, float height, bool shouldDraw)
{
    g.setFont(9.0f);
    g.setColour(juce::Colour(0xffe94560).withAlpha(0.5f));
    
    // Lambda to draw a single frequency marker
    auto drawMarker = [&](float freq, const juce::String& label)
    {
        if (freq < SpectrumScope::minFreq || freq > SpectrumScope::maxFreq) return;
        
        auto freqProportion = std::log(freq / SpectrumScope::minFreq) / std::log(SpectrumScope::maxFreq / SpectrumScope::minFreq);
        auto x = freqProportion * width;
        
        if (x >= 0 && x <= width)
        {
            g.drawLine(x, 0, x, height, 1.0f);
            g.drawText(label, static_cast<int>(x - 15), 2, 30, 12, juce::Justification::centred);
        }
    };
    
    // Draw standard frequency markers
    if(shouldDraw) {
        drawMarker(20.0f, "20");
        drawMarker(50.0f, "50");
        drawMarker(100.0f, "100");
        drawMarker(200.0f, "200");
        drawMarker(500.0f, "500");
        drawMarker(1000.0f, "1k");
        drawMarker(2000.0f, "2k");
        drawMarker(5000.0f, "5k");
        drawMarker(10000.0f, "10k");
        drawMarker(20000.0f, "20k");
    }
}

//==============================================================================
void SpectrumAnalyzer::timerCallback()
{
    if (scope.update(processor.getSpectrumTap(), processor.getSampleRate(), processor.isSpectrumBypassed()))
        repaint();
}
