#pragma once

#include <algorithm>
#include <cmath>

//==============================================================================
/** Turns the wet signal down while the dry one is playing, and lets it back up afterwards.
    It listens for the dry signal against its own recent level, so a quiet track ducks as a loud one does. */
class Ducker
{
public:
    void prepare (double sampleRate)
    {
        attack = coefficientFor (0.01, sampleRate);
        release = coefficientFor (0.25, sampleRate);
        recentRelease = coefficientFor (2.0, sampleRate);
        reset();
    }

    void reset()
    {
        envelope = 0.0f;
        recent = 0.0f;
    }

    /** The gain for the wet signal at this sample: 1 down to 1 - amount, while the dry signal is within 12 dB of its recent peak. */
    float gainFor (float dryPeak, float amount)
    {
        recent = std::max (dryPeak, recent + recentRelease * (dryPeak - recent));

        // How far towards full depth the dry signal is. No further, so the wet signal starts back up as soon as the dry one stops
        const auto fullDepthLevel = fullDepthRatio * std::max (recent, floorLevel);
        const auto depth = std::min (1.0f, dryPeak / fullDepthLevel);
        envelope += (depth > envelope ? attack : release) * (depth - envelope);
        return 1.0f - amount * envelope;
    }

private:
    static constexpr float fullDepthRatio = 0.25f;

    // -60 dB: under this the dry signal is noise, which doesn't duck
    static constexpr float floorLevel = 0.001f;

    static float coefficientFor (double seconds, double sampleRate)
    {
        return static_cast<float> (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
    }

    float attack = 1.0f, release = 1.0f, recentRelease = 1.0f;
    float envelope = 0.0f, recent = 0.0f;
};
