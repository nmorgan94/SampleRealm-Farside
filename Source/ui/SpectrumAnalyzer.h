#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/SpectrumFrameSource.h"
#include "FrameClock.h"

class AudioPluginAudioProcessor;

//==============================================================================
/** The wet signal's spectrum over the dry one's, with what the send's filters pass and how far the duck is holding the wet down. */
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer (AudioPluginAudioProcessor& proc);

    void paint (juce::Graphics& g) override;

private:
    static constexpr float duckBarWidth = 3.0f;

    // Half the filters' curve's width, so all of it shows at the top
    static constexpr float filtersInset = 0.5f;

    void timerCallback() override;

    juce::Path curve (int offset) const;
    juce::Path filtersCurve() const;

    AudioPluginAudioProcessor& processor;

    struct Cutoffs
    {
        float highpass = 0.0f, lowpass = 0.0f;
        bool operator== (const Cutoffs& other) const   { return juce::exactlyEqual (highpass, other.highpass) && juce::exactlyEqual (lowpass, other.lowpass); }
    };

    const std::atomic<float>& highpassFreq;
    const std::atomic<float>& lowpassFreq;

    SpectrumFrameSource source;
    FrameClock clock;
    Cutoffs shown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
