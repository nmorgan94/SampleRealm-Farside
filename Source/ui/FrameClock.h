#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/** How long it's been since it was last asked, for a display's timer. */
class FrameClock
{
public:
    double seconds()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        return (now - std::exchange (last, now)) / 1000.0;
    }

private:
    double last = juce::Time::getMillisecondCounterHiRes();
};
