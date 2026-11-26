#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include "ReverbParts.h"

//==============================================================================
/**
 * A hall: a long, smooth reverb that takes a moment to build. It is a feedback delay network,
 * after Jean-Marc Jot and Antoine Chaigne, "Digital delay networks for designing artificial
 * reverberators", AES 90th Convention, 1991: eight delays fed back through a Householder matrix,
 * each with the gain and lowpass that give every one the same decay. It takes the two sides' sum and gives stereo.
 */
class HallReverb
{
public:
    void prepare (double sampleRate);
    void reset();

    /** The time to fall 60 dB, the damping from 0 (open) to 1 (dark) and the width from 0 (mono) to 1. A change glides. */
    void setParameters (float decaySeconds, float damping, float width);

    /** Starts the next buffer at the parameters that are set. */
    void snap();

    void process (float* left, float* right, int numSamples);

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 3;

    struct Line
    {
        ReverbParts::DelayLine delay;
        ReverbParts::Lfo lfo;
        int length = 1;
        juce::SmoothedValue<float> gain { 0.0f };
        float damped = 0.0f;
    };

    double sampleRate = 48000.0;

    // The sum is smeared two ways before it goes in
    std::array<std::array<ReverbParts::Allpass, numDiffusers>, 2> diffusers;
    std::array<std::array<int, numDiffusers>, 2> diffuserLengths {};

    std::array<Line, numLines> lines;
    float excursion = 0.0f;
    juce::SmoothedValue<float> damping { 0.0f }, width { 1.0f };
};
