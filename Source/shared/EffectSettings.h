#pragma once

#include "ParameterSpecs.h"
#include "../dsp/Effect.h"

static_assert (ParameterSpecs::delayNotes.size() == Effect::delayNoteBeats.size());

//==============================================================================
template <typename ValueOf>
Effect::Settings makeEffectSettings (ValueOf&& valueOf)
{
    using namespace ParameterSpecs;

    Effect::Settings settings;
    settings.roomSize = valueOf (roomSize);
    settings.damping = valueOf (damping);
    settings.width = valueOf (width);
    settings.mix = valueOf (mix);
    settings.highpassFreq = valueOf (highpassFreq);
    settings.drive = valueOf (drive);
    settings.delaySync = valueOf (delaySync) >= 0.5f;
    settings.delayNote = juce::roundToInt (valueOf (delayNote));
    settings.delayTimeMs = valueOf (delayTime);
    settings.delayFeedback = valueOf (delayFeedback);
    settings.delayLevel = valueOf (delayLevel);
    settings.duck = valueOf (duck);
    settings.outputGainDb = valueOf (outputGain);
    return settings;
}
