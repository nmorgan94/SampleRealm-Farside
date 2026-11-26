#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

//==============================================================================
/** What the plate and the hall are built from, and what they share with juce::Reverb so the three answer to the same knobs. */
namespace ReverbParts
{
    /** How long juce::Reverb rings for at a room size, to -60 dB with no damping: its combs average 31 ms and feed back 0.7 to 0.98. */
    inline float decaySecondsFor (float roomSize)
    {
        constexpr float combSeconds = 1378.0f / 44100.0f;
        return -3.0f * combSeconds / std::log10 (0.7f + 0.28f * roomSize);
    }

    /** What a one-pole lowpass keeps of its last output, for a damping of 0 (open) to 1 (dark), at any sample rate. */
    inline float dampingCoefficientFor (float damping, double sampleRate)
    {
        const auto cutoff = std::min (20000.0 * std::pow (1500.0 / 20000.0, static_cast<double> (damping)), 0.45 * sampleRate);
        return static_cast<float> (std::exp (-2.0 * std::numbers::pi * cutoff / sampleRate));
    }

    // How long a change of decay, damping or width takes
    constexpr double glideSeconds = 0.02;

    /** juce::Reverb's width: how much of each side goes to itself and to the other. */
    struct WidthGains { float same, other; };

    constexpr WidthGains widthGainsFor (float width)
    {
        return { 0.5f + 0.5f * width, 0.5f - 0.5f * width };
    }

    //==============================================================================
    /** A delay read at any point behind where it's written. */
    class DelayLine
    {
    public:
        /** The longest delay it can be read at, in samples. */
        void setSize (int maxDelay)
        {
            samples.assign (static_cast<size_t> (maxDelay) + 2, 0.0f);
            index = 0;
        }

        void clear()   { std::fill (samples.begin(), samples.end(), 0.0f); }

        /** What was written that many writes ago, from 1. */
        float read (int delay) const
        {
            auto position = index - delay;

            if (position < 0)
                position += static_cast<int> (samples.size());

            return samples[static_cast<size_t> (position)];
        }

        /** Between two samples. */
        float read (float delay) const
        {
            const auto whole = static_cast<int> (delay);
            const auto newer = read (whole);
            return newer + (delay - static_cast<float> (whole)) * (read (whole + 1) - newer);
        }

        void write (float sample)
        {
            samples[static_cast<size_t> (index)] = sample;

            if (++index == static_cast<int> (samples.size()))
                index = 0;
        }

    private:
        std::vector<float> samples { 0.0f, 0.0f };
        int index = 0;
    };

    //==============================================================================
    /** An allpass that smears what goes through it over its length. */
    class Allpass
    {
    public:
        void setSize (int maxLength)   { line.setSize (maxLength); }
        void clear()                   { line.clear(); }

        /** With a length that's whole or between two samples. A negative coefficient turns it over. */
        template <typename Length>
        float process (float input, Length length, float coefficient)
        {
            const auto delayed = line.read (length);
            const auto fed = input - coefficient * delayed;
            line.write (fed);
            return delayed + coefficient * fed;
        }

        /** For a tap from inside it. */
        const DelayLine& getLine() const   { return line; }

    private:
        DelayLine line;
    };

    //==============================================================================
    /** A sine that turns slowly, for moving a delay's length. */
    class Lfo
    {
    public:
        void setRate (double hz, double sampleRate)   { turn = static_cast<float> (2.0 * std::sin (std::numbers::pi * hz / sampleRate)); }

        void reset (float startPhase)
        {
            sine = std::sin (startPhase);
            cosine = std::cos (startPhase);
        }

        float next()
        {
            // Each from the other, which keeps its size
            sine += turn * cosine;
            cosine -= turn * sine;
            return sine;
        }

    private:
        float sine = 0.0f, cosine = 1.0f, turn = 0.0f;
    };
}
