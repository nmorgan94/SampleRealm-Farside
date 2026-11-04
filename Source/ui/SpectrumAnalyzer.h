#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SpectrumScope.h"

// Forward declaration to avoid circular dependency
class AudioPluginAudioProcessor;

//==============================================================================
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer(AudioPluginAudioProcessor& proc);
    
    void paint(juce::Graphics& g) override;
    
private:
    // Timer callback runs on UI thread at 30Hz
    void timerCallback() override;
    
    // Draw frequency markers on the display
    void drawFrequencyMarkers(juce::Graphics& g, float width, float height, bool shouldDraw);
    
    // Reference to processor for accessing FIFO
    AudioPluginAudioProcessor& processor;
    
    SpectrumScope scope;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
