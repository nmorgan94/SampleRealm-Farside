#pragma once

#include "ParameterSpecs.h"
#include "../dsp/Effect.h"

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
    settings.sendGain = valueOf (sendGain);
    settings.highpassFreq = valueOf (highpassFreq);
    return settings;
}
