#pragma once

#include "SpectrumTap.h"
#include "../shared/SpectrumFrame.h"

//==============================================================================
/** Turns what SpectrumTap collects into the frame the display draws. One thread uses it. */
class SpectrumFrameSource
{
public:
    // After a longer gap, as while an editor was closed, what the tap holds is too old to show
    static constexpr double maxSeconds = 0.25;

    // With no audio for this long the host has stopped. Longer than any block it might send
    static constexpr double idleSeconds = 1.0;

    /** Reads the tap and moves the display on by a time. Returns true when getFrame() has changed. */
    bool update (SpectrumTap& tap, double seconds, double sampleRate, bool bypassed)
    {
        const auto last = frame;

        if (seconds > maxSeconds)
            discard (tap);

        if (bypassed || sampleRate <= 0.0)
        {
            discard (tap);
            rate = 0.0;
            frame = SpectrumFrame::silent();
            return frame != last;
        }

        if (! juce::exactlyEqual (sampleRate, rate))
        {
            // The first frame, or the first after a bypass
            discard (tap);
            rate = sampleRate;
            wet.setSampleRate (rate);
            dry.setSampleRate (rate);
        }

        seconds = std::clamp (seconds, 0.0, maxSeconds);
        auto heard = false;

        tap.pull ([&] (const float* drySamples, const float* wetSamples, int numSamples)
        {
            dry.push (drySamples, numSamples);
            wet.push (wetSamples, numSamples);
            heard = true;
        });

        idleFor = heard ? 0.0 : idleFor + seconds;

        if (heard)
            frame[SpectrumFrame::duckGain] = tap.readDuckGain();

        // The host has stopped sending audio, so everything falls away as if through silence
        if (idleFor > idleSeconds)
        {
            frame[SpectrumFrame::duckGain] = 1.0f;
            const std::array<float, 512> silence {};
            const auto numSamples = std::min (juce::roundToInt (seconds * rate), static_cast<int> (silence.size()));
            dry.push (silence.data(), numSamples);
            wet.push (silence.data(), numSamples);
        }

        wet.update (seconds);
        dry.update (seconds);

        std::copy (wet.getBands().begin(), wet.getBands().end(), frame.begin() + SpectrumFrame::wetOffset);
        std::copy (dry.getBands().begin(), dry.getBands().end(), frame.begin() + SpectrumFrame::dryOffset);
        return frame != last;
    }

    const SpectrumFrame::Data& getFrame() const noexcept   { return frame; }

private:
    static void discard (SpectrumTap& tap)
    {
        tap.pull ([] (const float*, const float*, int) {});
        tap.readDuckGain();
    }

    SpectrumFrame::Analyser wet, dry;
    double rate = 0.0;
    double idleFor = 0.0;
    SpectrumFrame::Data frame = SpectrumFrame::silent();
};
