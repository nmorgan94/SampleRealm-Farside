#pragma once

#include <algorithm>
#include <cmath>

//==============================================================================
/** Turns the wet signal down while the dry one is loud, and lets it back up afterwards. */
class Ducker
{
public:
    void prepare (double sampleRate)
    {
        attack = coefficientFor (0.01, sampleRate);
        release = coefficientFor (0.25, sampleRate);
        reset();
    }

    void reset()
    {
        envelope = 0.0f;
    }

    /** The gain for the wet signal at this sample: 1 down to 1 - amount, for a dry peak at or above -18 dB. */
    float gainFor (float dryPeak, float amount)
    {
        // No higher than full depth, so the wet signal starts back up as soon as the dry one stops
        const auto level = std::min (dryPeak, fullDepthLevel);
        envelope += (level > envelope ? attack : release) * (level - envelope);
        return 1.0f - amount * envelope / fullDepthLevel;
    }

private:
    static constexpr float fullDepthLevel = 0.125f;

    static float coefficientFor (double seconds, double sampleRate)
    {
        return static_cast<float> (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
    }

    float attack = 1.0f, release = 1.0f;
    float envelope = 0.0f;
};
