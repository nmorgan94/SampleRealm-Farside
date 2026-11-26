#pragma once

#include <cmath>
#include <utility>
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** A tanh soft clip blended in by its amount, which adds harmonics, not level: a signal at referenceLevel
    comes out at referenceLevel, a louder one quieter and a quieter one louder. At 0 the signal is untouched. */
class Drive
{
public:
    // -12 dB
    static constexpr float referenceLevel = 0.25f;

    /** 0 to 1. A change is spread over the next buffer. */
    void setAmount (float amount)
    {
        if (juce::exactlyEqual (amount, target.amount))
            return;

        // 1x to about 20x into the clip
        const auto gain = std::exp (amount * 3.0f);
        target = { amount, gain, referenceLevel / std::tanh (gain * referenceLevel) };
    }

    /** Starts the next buffer at the amount that's set. */
    void snap()   { current = target; }

    void process (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        auto shape = std::exchange (current, target);

        if (shape.amount <= 0.0f && target.amount <= 0.0f)
            return;

        const auto perSample = 1.0f / static_cast<float> (numSamples);
        const Shape step { (target.amount - shape.amount) * perSample, (target.gain - shape.gain) * perSample, (target.makeup - shape.makeup) * perSample };
        auto* const* channels = buffer.getArrayOfWritePointers();

        for (int i = 0; i < numSamples; ++i)
        {
            shape = { shape.amount + step.amount, shape.gain + step.gain, shape.makeup + step.makeup };

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto& sample = channels[channel][i];
                sample += shape.amount * (std::tanh (sample * shape.gain) * shape.makeup - sample);
            }
        }
    }

private:
    struct Shape { float amount = 0.0f, gain = 1.0f, makeup = 1.0f; };

    Shape current, target;
};
