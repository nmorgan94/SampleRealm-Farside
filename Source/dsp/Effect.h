#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/Transport.h>
#include "Drive.h"
#include "Ducker.h"
#include "PingPongDelay.h"
#include "SpectrumTap.h"

//==============================================================================
/** A send in one insert: the send is highpassed (24 dB/oct), driven and echoed into a 100% wet reverb.
    The echoes and the reverb are ducked by the dry signal, then blended with it, and the output gain is on the two together. */
class Effect
{
public:
    static constexpr int maxChannels = 2;

    // Beats per echo, in the order of ParameterSpecs::delayNotes
    static constexpr std::array delayNoteBeats { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };

    struct Settings
    {
        float roomSize = 0.8f;
        float damping = 0.8f;
        float width = 0.5f;
        float mix = 0.5f;
        float highpassFreq = 1000.0f;
        float drive = 0.0f;
        bool delaySync = true;
        int delayNote = 4;
        float delayTimeMs = 375.0f;
        float delayFeedback = 0.35f;
        float delayLevel = 0.0f;
        float duck = 0.0f;
        float outputGainDb = 0.0f;
    };

    /** Mono or stereo out, from as many channels or from mono. */
    void prepare (double sampleRate, int maxBlockSize, int numInputChannels, int numOutputChannels);
    void reset();
    void setSettings (const Settings& newSettings);

    /** Where the dry and wet signals go for the display, as they are in the output. Null for nowhere. */
    void setTap (SpectrumTap* newTap)   { tap = newTap; }

    /** As many channels as the output, of any length. A mono input into stereo is in the first. */
    void process (juce::AudioBuffer<float>& buffer, const Transport& transport);

private:
    // At or below this, the highpass is skipped
    static constexpr float highpassOffFreq = 20.0f;

    void processChunk (juce::AudioBuffer<float>& buffer);
    void updateHighpass();

    Settings settings;
    double sampleRate = 0.0;
    bool monoIntoStereo = false;

    juce::Reverb reverb;

    // Two cascaded 12 dB/oct stages per channel
    std::array<std::array<juce::SingleThreadedIIRFilter, maxChannels>, 2> highpass;
    float highpassCoefficientsFreq = 0.0f;

    Drive drive;
    PingPongDelay delay;
    Ducker ducker;
    float lastDelayLevel = 0.0f;
    float outputGain = 1.0f, lastOutputGain = 1.0f;
    bool ramping = false;

    juce::AudioBuffer<float> wet, echoes;

    SpectrumTap* tap = nullptr;
};
