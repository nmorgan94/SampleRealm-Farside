#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

//==============================================================================
/** A tanh soft clip blended in by its amount, which adds harmonics, not level: a signal at referenceLevel
    comes out at referenceLevel, a louder one quieter and a quieter one louder. At 0 the signal is untouched. */
class Drive
{
public:
    // -12 dB
    static constexpr float referenceLevel = 0.25f;

    /** 0 to 1. */
    void setAmount (float newAmount)
    {
        if (juce::exactlyEqual (newAmount, amount))
            return;

        amount = newAmount;
        // 1x to about 20x into the clip
        gain = std::exp (amount * 3.0f);
        makeup = referenceLevel / std::tanh (gain * referenceLevel);
    }

    void process (float* samples, int numSamples) const
    {
        if (amount <= 0.0f)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto clipped = std::tanh (samples[i] * gain) * makeup;
            samples[i] += amount * (clipped - samples[i]);
        }
    }

private:
    float amount = 0.0f;
    float gain = 1.0f;
    float makeup = 1.0f;
};
