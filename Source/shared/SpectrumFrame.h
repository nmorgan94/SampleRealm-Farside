#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <samplerealm/SpectrumAnalyser.h>

//==============================================================================
/**
 * What the display shows at one moment, as one flat array of floats: the duck's gain, then the
 * wet signal's bands and the dry signal's, in dB. SpectrumFrameSource fills it; the editors only
 * draw it. webui/src/spectrumFrame.ts has the same layout and scales.
 */
namespace SpectrumFrame
{
    inline constexpr int numBands = 96;

    // 1 with the wet signal untouched, down to 0: the lowest since the frame before
    inline constexpr int duckGain = 0;
    inline constexpr int wetOffset = 1;
    inline constexpr int dryOffset = wetOffset + numBands;
    inline constexpr int size = dryOffset + numBands;

    using Data = std::array<float, size>;
    using Analyser = SpectrumAnalyser<numBands>;

    inline constexpr float minHz = Analyser::minHz;
    inline constexpr float maxHz = Analyser::maxHz;
    inline constexpr float floorDb = Analyser::floorDb;

    // In dB after the analyser's tilt, which leaves music well under 0
    inline constexpr float topDb = -6.0f, bottomDb = -84.0f;

    inline constexpr int updateHz = 30;

    /** The frame with nothing heard. */
    constexpr Data silent()
    {
        Data frame {};
        std::fill (frame.begin(), frame.end(), floorDb);
        frame[duckGain] = 1.0f;
        return frame;
    }

    /** How far up the display a level is, from 0 at the bottom to 1. */
    constexpr float proportion (float db)
    {
        return std::clamp ((db - bottomDb) / (topDb - bottomDb), 0.0f, 1.0f);
    }

    /** How far across the display a frequency is, from 0 at minHz to 1. */
    inline float across (float hz)
    {
        return std::log (hz / minHz) / std::log (maxHz / minHz);
    }
}
