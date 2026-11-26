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
    highpass.prepare (sampleRate);
    lowpass.prepare (sampleRate);

    setSettings (settings);
    reset();
}

void Effect::reset()
{
    reverb.reset();
    delay.reset();
    ducker.reset();
    highpass.reset();
    lowpass.reset();
    ramping = false;
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

    const auto mix = mixGainsFor (settings.mix);
    const auto outputGain = juce::Decibels::decibelsToGain (settings.outputGainDb);
    gains = { mix.dry * outputGain, mix.wet * outputGain };

    highpass.setFrequency (settings.highpassFreq);
    lowpass.setFrequency (settings.lowpassFreq);
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
        lastGains = gains;
        drive.snap();
        delay.snap();
        highpass.snap();
        lowpass.snap();
        ramping = true;
    }

    for (int channel = 0; channel < numChannels; ++channel)
        wet.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    highpass.process (wet, numChannels, numSamples);
    lowpass.process (wet, numChannels, numSamples);
    drive.process (wet, numChannels, numSamples);

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

    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.applyGainRamp (channel, 0, numSamples, lastGains.dry, gains.dry);
        wet.applyGainRamp (channel, 0, numSamples, lastGains.wet, gains.wet);
    }

    if (tap != nullptr)
        tap->push (buffer, wet, numSamples, lowestDuckGain);

    for (int channel = 0; channel < numChannels; ++channel)
        buffer.addFrom (channel, 0, wet, channel, 0, numSamples);

    lastGains = gains;
}
