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
    inline const juce::ParameterID highpassFreqId   { ParameterSpecs::highpassFreq.id, versionHint };
    inline const juce::ParameterID driveId          { ParameterSpecs::drive.id, versionHint };
    inline const juce::ParameterID delaySyncId      { ParameterSpecs::delaySync.id, versionHint };
    inline const juce::ParameterID delayNoteId      { ParameterSpecs::delayNote.id, versionHint };
    inline const juce::ParameterID delayTimeId      { ParameterSpecs::delayTime.id, versionHint };
    inline const juce::ParameterID delayFeedbackId  { ParameterSpecs::delayFeedback.id, versionHint };
    inline const juce::ParameterID delayLevelId     { ParameterSpecs::delayLevel.id, versionHint };
    inline const juce::ParameterID duckId           { ParameterSpecs::duck.id, versionHint };
    inline const juce::ParameterID outputGainId     { ParameterSpecs::outputGain.id, versionHint };
    inline const juce::ParameterID spectrumBypassId { ParameterSpecs::spectrumBypass.id, versionHint };
}
