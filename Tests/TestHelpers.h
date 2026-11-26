#pragma once

#include <PluginTestHelpers.h>

#include "PluginEditor.h"

//==============================================================================
namespace TestHelpers
{
    using ProcessorFixture = PreparedProcessor<AudioPluginAudioProcessor>;

    // The largest step between one sample and the next
    inline float largestStep (const juce::AudioBuffer<float>& buffer)
    {
        auto largest = 0.0f;

        for (int i = 1; i < buffer.getNumSamples(); ++i)
            largest = std::max (largest, std::abs (buffer.getSample (0, i) - buffer.getSample (0, i - 1)));

        return largest;
    }
}
