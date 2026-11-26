#pragma once

#include <array>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** A highpass or lowpass of cascaded 12 dB/oct stages whose cutoff glides to where it's set.
    With the cutoff at the end of its range it's off, and it fades as it goes off and comes back on. */
template <int numStages>
class GlidingFilter
{
public:
    enum class Kind { highpass, lowpass };

    /** A highpass at or below offFrequency is off, as is a lowpass at or above it. */
    GlidingFilter (Kind kindToUse, float offFrequencyToUse)
        : kind (kindToUse), offFrequency (offFrequencyToUse), current (offFrequencyToUse), target (offFrequencyToUse) {}

    /** What one of these does to a frequency, in dB, away from half the sample rate. */
    static float responseDb (Kind kind, float offFrequency, float cutoff, float hz)
    {
        if (isOff (kind, offFrequency, cutoff))
            return 0.0f;

        // How far into the passband, as a ratio
        const auto ratio = kind == Kind::highpass ? hz / cutoff : cutoff / hz;
        return numStages * -10.0f * std::log10 (1.0f + std::pow (ratio, -4.0f));
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        glide = static_cast<float> (1.0 - std::exp (-stepSize / (glideSeconds * sampleRate)));
        reset();
    }

    void reset()
    {
        snap();

        for (auto& stage : filters)
            for (auto& filter : stage)
                filter.reset();
    }

    void setFrequency (float hz)   { target = hz; }

    /** Starts the next buffer at the cutoff that's set. */
    void snap()
    {
        current = target;
        fade = isOff (current) ? 0.0f : 1.0f;
        updateCoefficients();
    }

    void process (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        for (int start = 0; start < numSamples;)
        {
            const auto moving = ! juce::exactlyEqual (current, target);
            const auto off = isOff (current) && isOff (target);

            // Off and faded out: the rest is left alone
            if (off && fade <= 0.0f)
                return;

            // A few samples at a time while anything is changing, then the rest at once
            const auto fading = fade < 1.0f || off;
            const auto length = moving || fading ? std::min (stepSize, numSamples - start) : numSamples - start;

            if (moving)
            {
                // Evenly in pitch
                current *= std::pow (target / current, glide);

                if (std::abs (current / target - 1.0f) < 1.0e-3f)
                    current = target;

                updateCoefficients();
            }

            // Faded in as it comes on and out as it goes off, as it starts from rest and ends away from the unfiltered signal
            const auto fadeStep = (off ? -1.0f : 1.0f) / fadeSamples;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* samples = buffer.getWritePointer (channel, start);
                std::array<float, stepSize> unfiltered;

                if (fading)
                    std::copy_n (samples, length, unfiltered.begin());

                for (auto& stage : filters)
                    stage[static_cast<size_t> (channel)].processSamples (samples, length);

                if (fading)
                {
                    for (int i = 0; i < length; ++i)
                    {
                        const auto amount = std::clamp (fade + fadeStep * static_cast<float> (i + 1), 0.0f, 1.0f);
                        samples[i] = unfiltered[static_cast<size_t> (i)] + (samples[i] - unfiltered[static_cast<size_t> (i)]) * amount;
                    }
                }
            }

            if (fading)
                fade = std::clamp (fade + fadeStep * static_cast<float> (length), 0.0f, 1.0f);

            // So it comes back on from rest
            if (off && fade <= 0.0f)
                for (auto& stage : filters)
                    for (auto& filter : stage)
                        filter.reset();

            start += length;
        }
    }

private:
    static constexpr int maxChannels = 2;

    // How often a moving cutoff is set, in samples, and how long it takes to get most of the way
    static constexpr int stepSize = 4;
    static constexpr double glideSeconds = 0.03;

    // How long it takes to fade in or out, in samples
    static constexpr float fadeSamples = 32.0f;

    static bool isOff (Kind kind, float offFrequency, float hz)   { return kind == Kind::highpass ? hz <= offFrequency : hz >= offFrequency; }
    bool isOff (float hz) const                                   { return isOff (kind, offFrequency, hz); }

    void updateCoefficients()
    {
        // Unstable at or above Nyquist
        const auto frequency = std::min<double> (current, sampleRate * 0.49);
        const auto coefficients = kind == Kind::highpass ? juce::IIRCoefficients::makeHighPass (sampleRate, frequency)
                                                         : juce::IIRCoefficients::makeLowPass (sampleRate, frequency);

        for (auto& stage : filters)
            for (auto& filter : stage)
                filter.setCoefficients (coefficients);
    }

    Kind kind;
    float offFrequency;
    double sampleRate = 48000.0;
    float glide = 1.0f;
    float current, target;

    // How much of the filtered signal is heard, from 0 while it's off
    float fade = 0.0f;

    std::array<std::array<juce::SingleThreadedIIRFilter, maxChannels>, numStages> filters;
};
