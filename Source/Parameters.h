#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/ParameterSpecs.h"

//==============================================================================
namespace Parameters
{
    // Bump for parameters added after a release
    constexpr int versionHint = 1;

    inline const juce::ParameterID roomSizeId       { ParameterSpecs::roomSize.id, versionHint };
    inline const juce::ParameterID dampingId        { ParameterSpecs::damping.id, versionHint };
    inline const juce::ParameterID widthId          { ParameterSpecs::width.id, versionHint };
    inline const juce::ParameterID mixId            { ParameterSpecs::mix.id, versionHint };
    inline const juce::ParameterID sendGainId       { ParameterSpecs::sendGain.id, versionHint };
    inline const juce::ParameterID highpassFreqId   { ParameterSpecs::highpassFreq.id, versionHint };
    inline const juce::ParameterID spectrumBypassId { ParameterSpecs::spectrumBypass.id, versionHint };
}
