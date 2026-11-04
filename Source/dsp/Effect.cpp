#include "Effect.h"

//==============================================================================
void Effect::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;
    reverb.setSampleRate (sampleRate);

    wet.setSize (numChannels, maxBlockSize);

    reset();
    highpassCoefficientsFreq = 0.0f;
    updateHighpass();
}

void Effect::reset()
{
    reverb.reset();

    for (auto& stage : highpass)
        for (auto& filter : stage)
            filter.reset();
}

void Effect::setSettings (const Settings& newSettings)
{
    settings = newSettings;

    juce::Reverb::Parameters parameters;
    parameters.roomSize = settings.roomSize;
    parameters.damping = settings.damping;
    parameters.width = settings.width;
    parameters.wetLevel = 1.0f;
    parameters.dryLevel = 0.0f;
    reverb.setParameters (parameters);

    updateHighpass();
}

void Effect::updateHighpass()
{
    if (sampleRate <= 0.0 || juce::exactlyEqual (settings.highpassFreq, highpassCoefficientsFreq))
        return;

    highpassCoefficientsFreq = settings.highpassFreq;
    // Unstable at or above Nyquist
    const auto frequency = std::min<double> (settings.highpassFreq, sampleRate * 0.49);
    const auto coefficients = juce::IIRCoefficients::makeHighPass (sampleRate, frequency);

    for (auto& stage : highpass)
        for (auto& filter : stage)
            filter.setCoefficients (coefficients);
}

void Effect::process (juce::AudioBuffer<float>& buffer, const Transport& transport)
{
    juce::ignoreUnused (transport);

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel)
        wet.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    if (settings.highpassFreq > highpassOffFreq)
        for (auto& stage : highpass)
            for (int channel = 0; channel < numChannels; ++channel)
                stage[static_cast<size_t> (channel)].processSamples (wet.getWritePointer (channel), numSamples);

    wet.applyGain (0, numSamples, settings.sendGain);

    if (numChannels == 1)
        reverb.processMono (wet.getWritePointer (0), numSamples);
    else if (numChannels == 2)
        reverb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), numSamples);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.applyGain (channel, 0, numSamples, 1.0f - settings.mix);
        buffer.addFrom (channel, 0, wet, channel, 0, numSamples, settings.mix);
    }
}
