#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** Hands the output, mixed to mono, from the audio thread to the spectrum display without locking. */
class SpectrumTap
{
public:
    static constexpr int capacity = 48000;

    SpectrumTap() = default;

    void reset() noexcept
    {
        fifo.reset();
    }

    void push (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto numChannels = buffer.getNumChannels();
        int start1, size1, start2, size2;
        fifo.prepareToWrite (buffer.getNumSamples(), start1, size1, start2, size2);

        const auto writeRegion = [&] (int fifoStart, int regionSize, int bufferOffset)
        {
            for (int i = 0; i < regionSize; ++i)
            {
                float sample = 0.0f;

                for (int channel = 0; channel < numChannels; ++channel)
                    sample += buffer.getSample (channel, bufferOffset + i);

                samples.setSample (0, fifoStart + i, sample / static_cast<float> (numChannels));
            }
        };

        writeRegion (start1, size1, 0);
        writeRegion (start2, size2, size1);

        fifo.finishedWrite (size1 + size2);
    }

    /** Calls consume (const float*, int) with everything pushed since the last pull, in order. */
    template <typename Consume>
    void pull (Consume&& consume)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (fifo.getNumReady(), start1, size1, start2, size2);

        if (size1 > 0) consume (samples.getReadPointer (0, start1), size1);
        if (size2 > 0) consume (samples.getReadPointer (0, start2), size2);

        fifo.finishedRead (size1 + size2);
    }

private:
    juce::AbstractFifo fifo { capacity };
    juce::AudioBuffer<float> samples { 1, capacity };

    JUCE_DECLARE_NON_COPYABLE (SpectrumTap)
};
