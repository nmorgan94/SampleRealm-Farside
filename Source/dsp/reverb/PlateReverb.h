#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include "ReverbParts.h"

//==============================================================================
/**
 * A plate: a bright, dense reverb with no early echoes. The algorithm is Jon Dattorro's, from
 * "Effect Design, Part 1: Reverberator and Other Filters", J. Audio Eng. Soc. 45 (9), 1997,
 * figure 1 and table 1, written here from the paper. It takes the two sides' sum and gives stereo.
 */
class PlateReverb
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
    // One side of the tank, which feeds the other
    struct Half
    {
        ReverbParts::Allpass modulated, diffuser;
        ReverbParts::DelayLine first, second;
        ReverbParts::Lfo lfo;
        int modulatedLength = 1, diffuserLength = 1, firstLength = 1, secondLength = 1;
        float damped = 0.0f;
    };

    int scaled (int samplesInThePaper) const;
    void throughHalf (Half& half, float input, float decayNow, float dampingNow);

    double sampleRate = 48000.0;

    std::array<ReverbParts::Allpass, 4> inputDiffusers;
    std::array<int, 4> inputLengths {};
    float bandwidthState = 0.0f;

    Half leftHalf, rightHalf;
    float excursion = 0.0f;

    // Where the output is read from, in the paper's order
    std::array<int, 7> leftTaps {}, rightTaps {};

    juce::SmoothedValue<float> decay { 0.5f }, damping { 0.0f }, width { 1.0f };
};
