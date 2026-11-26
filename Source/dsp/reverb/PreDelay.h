#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include "ReverbParts.h"

//==============================================================================
/** Holds the send back before the reverb. A change glides. At 0 it passes the send straight through. */
class PreDelay
{
public:
    static constexpr double maxSeconds = 0.25;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        for (auto& line : lines)
            line.setSize (juce::roundToInt (std::ceil (maxSeconds * sampleRate)) + 1);

        delayInSamples.reset (sampleRate, 0.05);
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
            line.clear();

        snap();
    }

    void setTime (double seconds)   { delayInSamples.setTargetValue (static_cast<float> (std::clamp (seconds, 0.0, maxSeconds) * sampleRate)); }

    /** Starts the next buffer at the time that's set. */
    void snap()   { delayInSamples.setCurrentAndTargetValue (delayInSamples.getTargetValue()); }

    void process (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto delay = delayInSamples.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto& line = lines[static_cast<size_t> (channel)];
                auto& sample = buffer.getWritePointer (channel)[i];

                // Written first, so a delay of 0 reads it straight back
                line.write (sample);
                sample = line.read (delay + 1.0f);
            }
        }
    }

private:
    double sampleRate = 48000.0;
    std::array<ReverbParts::DelayLine, 2> lines;
    juce::SmoothedValue<float> delayInSamples;
};
