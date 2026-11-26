#include "Effect.h"

//==============================================================================
void Effect::prepare (double newSampleRate, int maxBlockSize, int numInputChannels, int numOutputChannels)
{
    sampleRate = newSampleRate;
    monoIntoStereo = numInputChannels == 1 && numOutputChannels == 2;
    reverb.setSampleRate (sampleRate);

    wet.setSize (numOutputChannels, maxBlockSize);
    echoes.setSize (numOutputChannels, maxBlockSize);
    delay.prepare (sampleRate, numOutputChannels);
    ducker.prepare (sampleRate);

    reset();
    highpassCoefficientsFreq = 0.0f;
    updateHighpass();
}

void Effect::reset()
{
    reverb.reset();
    delay.reset();
    ducker.reset();
    ramping = false;

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

    drive.setAmount (settings.drive);
    delay.setFeedback (settings.delayFeedback);
    outputGain = juce::Decibels::decibelsToGain (settings.outputGainDb);

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
    if (monoIntoStereo)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    const auto chunkSize = wet.getNumSamples();

    if (chunkSize == 0)
        return;

    const auto note = static_cast<size_t> (std::clamp (settings.delayNote, 0, static_cast<int> (delayNoteBeats.size()) - 1));
    delay.setTime (settings.delaySync ? delayNoteBeats[note] * 60.0 / std::max (1.0, transport.bpm)
                                      : settings.delayTimeMs / 1000.0);

    // Hosts can send more than they prepared for
    for (int start = 0; start < buffer.getNumSamples(); start += chunkSize)
    {
        juce::AudioBuffer<float> chunk { buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start,
                                         std::min (chunkSize, buffer.getNumSamples() - start) };
        processChunk (chunk);
    }
}

void Effect::processChunk (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    // The first chunk after a reset starts at its settings
    if (! ramping)
    {
        lastDelayLevel = settings.delayLevel;
        lastOutputGain = outputGain;
        ramping = true;
    }

    for (int channel = 0; channel < numChannels; ++channel)
        wet.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    if (settings.highpassFreq > highpassOffFreq)
        for (auto& stage : highpass)
            for (int channel = 0; channel < numChannels; ++channel)
                stage[static_cast<size_t> (channel)].processSamples (wet.getWritePointer (channel), numSamples);

    for (int channel = 0; channel < numChannels; ++channel)
        drive.process (wet.getWritePointer (channel), numSamples);

    // The line always runs, so its echoes are there when the level comes up
    juce::AudioBuffer<float> echoesNow { echoes.getArrayOfWritePointers(), numChannels, numSamples };

    for (int channel = 0; channel < numChannels; ++channel)
        echoesNow.copyFrom (channel, 0, wet, channel, 0, numSamples);

    delay.process (echoesNow);

    const auto hasEchoes = lastDelayLevel > 0.0f || settings.delayLevel > 0.0f;

    // The echoes go into the reverb, and are heard themselves
    auto addEchoes = [&]
    {
        if (hasEchoes)
            for (int channel = 0; channel < numChannels; ++channel)
                wet.addFromWithRamp (channel, 0, echoesNow.getReadPointer (channel), numSamples, lastDelayLevel, settings.delayLevel);
    };

    addEchoes();

    if (numChannels == 1)
        reverb.processMono (wet.getWritePointer (0), numSamples);
    else if (numChannels == 2)
        reverb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), numSamples);

    addEchoes();
    lastDelayLevel = settings.delayLevel;

    auto lowestDuckGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        auto dryPeak = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            dryPeak = std::max (dryPeak, std::abs (buffer.getSample (channel, i)));

        const auto gain = ducker.gainFor (dryPeak, settings.duck);

        if (settings.duck > 0.0f)
        {
            lowestDuckGain = std::min (lowestDuckGain, gain);

            for (int channel = 0; channel < numChannels; ++channel)
                wet.getWritePointer (channel)[i] *= gain;
        }
    }

    const auto dryGain = 1.0f - settings.mix;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.applyGainRamp (channel, 0, numSamples, dryGain * lastOutputGain, dryGain * outputGain);
        wet.applyGainRamp (channel, 0, numSamples, settings.mix * lastOutputGain, settings.mix * outputGain);
    }

    if (tap != nullptr)
        tap->push (buffer, wet, numSamples, lowestDuckGain);

    for (int channel = 0; channel < numChannels; ++channel)
        buffer.addFrom (channel, 0, wet, channel, 0, numSamples);

    lastOutputGain = outputGain;
}
