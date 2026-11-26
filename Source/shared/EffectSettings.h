#pragma once

#include "ParameterSpecs.h"
#include "../dsp/Effect.h"

static_assert (ParameterSpecs::delayNotes.size() == Effect::delayNoteBeats.size());
static_assert (ParameterSpecs::reverbTypes.size() == Effect::numReverbTypes);
static_assert (juce::exactlyEqual (ParameterSpecs::highpassFreq.min, Effect::highpassOffFreq) && juce::exactlyEqual (ParameterSpecs::lowpassFreq.max, Effect::lowpassOffFreq));

//==============================================================================
template <typename ValueOf>
Effect::Settings makeEffectSettings (ValueOf&& valueOf)
{
    using namespace ParameterSpecs;

    Effect::Settings settings;
    settings.reverbType = juce::roundToInt (valueOf (reverbType));
    settings.preDelayMs = valueOf (preDelay);
    settings.roomSize = valueOf (roomSize);
    settings.damping = valueOf (damping);
    settings.width = valueOf (width);
    settings.mix = valueOf (mix);
    settings.highpassFreq = valueOf (highpassFreq);
    settings.lowpassFreq = valueOf (lowpassFreq);
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
