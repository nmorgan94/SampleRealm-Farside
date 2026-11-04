#pragma once

#include <array>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/SpectrumTap.h"

//==============================================================================
/** Turns what SpectrumTap collects into the 512 points the display draws, 0 to 1 on a log frequency scale. */
class SpectrumScope
{
public:
    static constexpr auto fftOrder = 11;
    static constexpr auto fftSize = 1 << fftOrder;
    static constexpr auto scopeSize = 512;

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr int updateHz = 30;

    using Scope = std::array<float, scopeSize>;

    /** Reads the tap, and returns true when getScope() has changed: a new frame, or cleared on bypass. */
    bool update (SpectrumTap& tap, double sampleRate, bool bypassed);

    const Scope& getScope() const noexcept   { return scope; }

private:
    void pushSamples (const float* samples, int numSamples);
    void computeScope (double sampleRate);

    juce::dsp::FFT forwardFFT { fftOrder };

    // Twice the size, as performFrequencyOnlyForwardTransform needs
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> fifo {};
    int fifoIndex = 0;
    bool nextBlockReady = false;

    Scope scope {};
    bool cleared = false;
};
