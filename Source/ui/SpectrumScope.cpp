#include "SpectrumScope.h"

//==============================================================================
bool SpectrumScope::update (SpectrumTap& tap, double sampleRate, bool bypassed)
{
    if (bypassed)
    {
        if (cleared)
            return false;

        scope.fill (0.0f);
        cleared = true;
        return true;
    }

    tap.pull ([this] (const float* samples, int numSamples) { pushSamples (samples, numSamples); });

    if (! nextBlockReady)
        return false;

    computeScope (sampleRate);
    nextBlockReady = false;
    cleared = false;
    return true;
}

void SpectrumScope::pushSamples (const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        fifo[static_cast<size_t> (fifoIndex++)] = samples[i];

        if (fifoIndex == fftSize)
        {
            fftData.fill (0.0f);
            std::copy (fifo.begin(), fifo.end(), fftData.begin());
            nextBlockReady = true;
            fifoIndex = 0;
        }
    }
}

void SpectrumScope::computeScope (double sampleRate)
{
    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

    const auto maxMagnitude = juce::jmax (juce::FloatVectorOperations::findMaximum (fftData.data(), fftSize / 2), 1e-5f);
    const auto nyquist = static_cast<float> (sampleRate) / 2.0f;

    for (int i = 0; i < scopeSize; ++i)
    {
        const auto proportion = static_cast<float> (i) / static_cast<float> (scopeSize - 1);
        const auto freq = minFreq * std::pow (maxFreq / minFreq, proportion);

        const auto bin = (freq / nyquist) * (fftSize / 2);
        const auto binIndex = juce::jlimit (0, fftSize / 2 - 2, static_cast<int> (bin));
        const auto binFraction = bin - static_cast<float> (static_cast<int> (bin));

        const auto low = fftData[static_cast<size_t> (binIndex)];
        const auto high = fftData[static_cast<size_t> (binIndex + 1)];

        scope[static_cast<size_t> (i)] = (low + binFraction * (high - low)) / maxMagnitude;
    }
}
