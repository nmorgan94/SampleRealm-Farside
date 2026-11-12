#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/Transport.h>

//==============================================================================
/** Send-style reverb: the send is highpassed (24 dB/oct), gained, reverbed 100% wet and blended with the dry. */
class Effect
{
public:
    static constexpr int maxChannels = 2;

    struct Settings
    {
        float roomSize = 0.8f;
        float damping = 0.8f;
        float width = 0.5f;
        float mix = 0.5f;
        float sendGain = 1.0f;
        float highpassFreq = 1000.0f;
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const Settings& newSettings);

    /** Mono or stereo, no longer than maxBlockSize. */
    void process (juce::AudioBuffer<float>& buffer, const Transport& transport);

private:
    // At or below this, the highpass is skipped
    static constexpr float highpassOffFreq = 20.0f;

    void updateHighpass();

    Settings settings;
    double sampleRate = 0.0;

    juce::Reverb reverb;

    // Two cascaded 12 dB/oct stages per channel
    std::array<std::array<juce::SingleThreadedIIRFilter, maxChannels>, 2> highpass;
    float highpassCoefficientsFreq = 0.0f;

    juce::AudioBuffer<float> wet;
};
