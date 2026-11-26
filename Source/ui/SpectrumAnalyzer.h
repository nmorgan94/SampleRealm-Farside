#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/SpectrumFrameSource.h"
#include "FrameClock.h"

class AudioPluginAudioProcessor;

//==============================================================================
/** The wet signal's spectrum over the dry one's, with the highpass's cutoff and how far the duck is holding the wet down. */
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer (AudioPluginAudioProcessor& proc);

    void paint (juce::Graphics& g) override;

private:
    static constexpr float duckBarWidth = 3.0f;

    void timerCallback() override;

    juce::Path curve (int offset) const;

    AudioPluginAudioProcessor& processor;

    const std::atomic<float>& highpassFreq;

    SpectrumFrameSource source;
    FrameClock clock;
    float shownHighpassFreq = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
