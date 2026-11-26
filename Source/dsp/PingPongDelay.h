#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** Echoes that alternate left and right in stereo, starting on the left, or a single line in mono.
    Each repeat is a little darker than the last. */
class PingPongDelay
{
public:
    static constexpr double minSeconds = 0.01, maxSeconds = 4.0;

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Starts the next buffer at the time and feedback that are set. */
    void snap();

    /** Kept within minSeconds and maxSeconds. A change glides, as a tape delay's does. */
    void setTime (double seconds);

    /** A change is spread over a few milliseconds. */
    void setFeedback (float newFeedback);

    /** Replaces the buffer with its echoes alone. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    static constexpr int maxChannels = 2;

    double sampleRate = 0.0;
    juce::AudioBuffer<float> lines;
    int writeIndex = 0;

    juce::SmoothedValue<float> delayInSamples, feedback;

    // One-pole lowpass on what each line feeds on
    float dampingCoefficient = 1.0f;
    std::array<float, maxChannels> damped {};
};
