#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** Hands the dry and the wet signals, each mixed to mono, and the duck's gain from the audio thread
    to the spectrum display without locking. With no reader it fills up, and the rest is dropped. */
class SpectrumTap
{
public:
    static constexpr int capacity = 48000;

    SpectrumTap() = default;

    void reset() noexcept
    {
        fifo.reset();
        duckGain = 1.0f;
    }

    /** The next samples of each, in as many channels as the dry signal has, and the lowest gain the duck gave the wet signal over them. */
    void push (const juce::AudioBuffer<float>& dry, const juce::AudioBuffer<float>& wet, int numSamples, float lowestDuckGain) noexcept
    {
        const auto scope = fifo.write (numSamples);
        const auto numChannels = dry.getNumChannels();
        const auto gain = 1.0f / static_cast<float> (numChannels);

        const auto mix = [&] (int destination, const juce::AudioBuffer<float>& source, int fifoStart, int sourceStart, int length)
        {
            if (length == 0)
                return;

            samples.copyFrom (destination, fifoStart, source.getReadPointer (0, sourceStart), length, gain);

            for (int channel = 1; channel < numChannels; ++channel)
                samples.addFrom (destination, fifoStart, source, channel, sourceStart, length, gain);
        };

        for (const auto [destination, source] : { std::pair { 0, &dry }, std::pair { 1, &wet } })
        {
            mix (destination, *source, scope.startIndex1, 0, scope.blockSize1);
            mix (destination, *source, scope.startIndex2, scope.blockSize1, scope.blockSize2);
        }

        auto current = duckGain.load();
        while (lowestDuckGain < current && ! duckGain.compare_exchange_weak (current, lowestDuckGain)) {}
    }

    /** Calls consume (const float* dry, const float* wet, int) with everything pushed since the last pull, in order. */
    template <typename Consume>
    void pull (Consume&& consume)
    {
        const auto scope = fifo.read (fifo.getNumReady());

        if (scope.blockSize1 > 0) consume (samples.getReadPointer (0, scope.startIndex1), samples.getReadPointer (1, scope.startIndex1), scope.blockSize1);
        if (scope.blockSize2 > 0) consume (samples.getReadPointer (0, scope.startIndex2), samples.getReadPointer (1, scope.startIndex2), scope.blockSize2);
    }

    /** The lowest duck gain pushed since the last call. */
    float readDuckGain() noexcept   { return duckGain.exchange (1.0f); }

private:
    juce::AbstractFifo fifo { capacity };
    juce::AudioBuffer<float> samples { 2, capacity };
    std::atomic<float> duckGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE (SpectrumTap)
};
